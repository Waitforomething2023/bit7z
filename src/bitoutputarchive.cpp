// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/*
 * bit7z - A C++ static library to interface with the 7-zip DLLs.
 * Copyright (c) 2014-2021  Riccardo Ostani - All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Bit7z is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bit7z; if not, see https://www.gnu.org/licenses/.
 */

#include "bitoutputarchive.hpp"

#include "bitexception.hpp"
#include "fs.hpp"
#include "fsitem.hpp"
#include "fsindexer.hpp"
#include "bufferitem.hpp"
#include "streamitem.hpp"
#include "updatecallback.hpp"
#include "cmultivoloutstream.hpp"
#include "cbufferoutstream.hpp"

using bit7z::BitException;
using bit7z::BitOutputArchive;
using bit7z::BitArchiveCreator;
using bit7z::BitArchiveHandler;
using bit7z::BitPropVariant;
using bit7z::filesystem::FSItem;
using bit7z::BufferItem;
using bit7z::StreamItem;
using bit7z::filesystem::FSIndexer;
using bit7z::UpdateCallback;
using bit7z::byte_t;
using bit7z::tstring;
using bit7z::input_index;

BitOutputArchive::BitOutputArchive( const BitArchiveCreator& creator, tstring in_file )
    : mInputArchiveItemsCount{ 0 }, mArchiveCreator{ creator } {
    std::error_code ec;
    if ( !in_file.empty() && fs::exists( in_file, ec ) ) {
        if ( mArchiveCreator.updateMode() == UpdateMode::NONE ) {
            throw BitException( "Archive creator cannot update archives",
                                std::make_error_code( std::errc::invalid_argument ) );
        }
        if ( !mArchiveCreator.compressionFormat().hasFeature( FormatFeatures::MULTIPLE_FILES ) ) {
            //Update mode is set but format does not support adding more files
            throw BitException( "Format does not support updating existing archive files",
                                std::make_error_code( std::errc::invalid_argument ) );
        }
        mInputArchive = std::make_unique< BitInputArchive >( creator, std::move( in_file ) );
        mInputArchiveItemsCount = mInputArchive->itemsCount();
    }
}

BitOutputArchive::BitOutputArchive( const BitArchiveCreator& creator,
                                    const std::vector< bit7z::byte_t >& in_buffer )
    :  mInputArchiveItemsCount{ 0 }, mArchiveCreator{ creator } {
    if ( !in_buffer.empty() ) {
        mInputArchive = std::make_unique< BitInputArchive >( creator, in_buffer );
        mInputArchiveItemsCount = mInputArchive->itemsCount();
    }
}

BitOutputArchive::BitOutputArchive( const BitArchiveCreator& creator, std::istream& in_stream )
    : mInputArchiveItemsCount{ 0 }, mArchiveCreator{ creator } {
    mInputArchive = std::make_unique< BitInputArchive >( creator, in_stream );
    mInputArchiveItemsCount = mInputArchive->itemsCount();
}

void BitOutputArchive::addItems( const std::vector< tstring >& in_paths ) {
    mNewItemsVector.indexPaths( in_paths );
}

void BitOutputArchive::addItems( const std::map< tstring, tstring >& in_paths ) {
    mNewItemsVector.indexPathsMap( in_paths );
}

void BitOutputArchive::addFile( const tstring& in_file, const tstring& name ) {
    mNewItemsVector.indexFile( in_file, name );
}

void BitOutputArchive::addFile( const std::vector< byte_t >& in_buffer, const tstring& name ) {
    mNewItemsVector.indexBuffer( in_buffer, name );
}

void BitOutputArchive::addFile( std::istream& in_stream, const tstring& name ) {
    mNewItemsVector.indexStream( in_stream, name );
}

void BitOutputArchive::addFiles( const std::vector< tstring >& in_files ) {
    mNewItemsVector.indexPaths( in_files, true );
}


void BitOutputArchive::addFiles( const tstring& in_dir, bool recursive, const tstring& filter ) {
    mNewItemsVector.indexDirectory( in_dir, filter, recursive );
}

void BitOutputArchive::addDirectory( const tstring& in_dir ) {
    mNewItemsVector.indexDirectory( in_dir );
}

void BitOutputArchive::compressTo( const tstring& out_file ) {
    CMyComPtr< UpdateCallback > update_callback = new UpdateCallback( *this );
    compressToFile( out_file, update_callback );
}

CMyComPtr< IOutArchive > BitOutputArchive::initOutArchive() const {
    CMyComPtr< IOutArchive > new_arc;
    if ( mInputArchive == nullptr ) {
        const GUID format_GUID = mArchiveCreator.format().guid();
        mArchiveCreator.library()
                       .createArchiveObject( &format_GUID, &::IID_IOutArchive, reinterpret_cast< void** >( &new_arc ) );
    } else {
        mInputArchive->initUpdatableArchive( &new_arc );
    }
    setArchiveProperties( new_arc );
    return new_arc;
}

CMyComPtr< IOutStream > BitOutputArchive::initOutFileStream( const tstring& out_archive,
                                                             bool updating_archive ) const {
    if ( mArchiveCreator.volumeSize() > 0 ) {
        return new CMultiVolOutStream( mArchiveCreator.volumeSize(), out_archive );
    }

    fs::path out_path = out_archive;
    if ( updating_archive ) {
        out_path += ".tmp";
    }

    auto* file_out_stream = new CFileOutStream( out_path, updating_archive );
    CMyComPtr< IOutStream > out_stream = file_out_stream;
    if ( file_out_stream->fail() ) {
        //Unknown error!
        throw BitException( "Cannot create output archive file", last_error_code(), out_path.native() );
    }
    return out_stream;
}

void BitOutputArchive::compressOut( IOutArchive* out_arc,
                                    IOutStream* out_stream,
                                    UpdateCallback* update_callback ) {
    if ( mInputArchive != nullptr && mArchiveCreator.updateMode() == UpdateMode::OVERWRITE ) {
        for ( auto& new_item : mNewItemsVector ) {
            auto overwritten_item = mInputArchive->find( new_item->inArchivePath() );
            if ( overwritten_item != mInputArchive->cend() ) {
                mDeletedItems.insert( overwritten_item->index() );
            }
        }
    }
    updateInputIndices();

    HRESULT result = out_arc->UpdateItems( out_stream, itemsCount(), update_callback );

    if ( result == E_NOTIMPL ) {
        throw BitException( bit7z::kUnsupportedOperation, bit7z::make_hresult_code( result ) );
    }

    if ( result != S_OK ) {
        throw BitException( "Error compressing files", make_hresult_code( result ), std::move( mFailedFiles ) );
    }
}

void BitOutputArchive::compressToFile( const tstring& out_file, UpdateCallback* update_callback ) {
    // Note: if old_arc != nullptr, new_arc will actually point to the same IInArchive object used by old_arc
    // (see initUpdatableArchive function of BitInputArchive)!
    bool updating_archive = mInputArchive != nullptr && mInputArchive->getArchivePath() == out_file;
    CMyComPtr< IOutArchive > new_arc = initOutArchive();
    CMyComPtr< IOutStream > out_stream = initOutFileStream( out_file, updating_archive );
    compressOut( new_arc, out_stream, update_callback );

    if ( updating_archive ) { //we updated the input archive
        mInputArchive->close();
        /* NOTE: In the following instruction, we use the . (dot) operator, not the -> (arrow) operator:
         *       in fact, both CMyComPtr and IOutStream have a Release() method, and we need to call only
         *       the one of CMyComPtr (which in turns calls the one of IOutStream)! */
        out_stream.Release(); //Releasing the output stream so that we can rename it as the original file

#if defined( __MINGW32__ ) && defined( USE_STANDARD_FILESYSTEM )
        /* MinGW seems to not follow the standard since filesystem::rename does not overwrite an already
         * existing destination file (as it should). So we explicitly remove it before! */
        std::error_code ec;
        fs::remove( out_file, ec );
        if ( ec ) {
            throw BitException( "Cannot remove old archive file", ec, out_file );
        }
#endif

        //remove old file and rename tmp file (move file with overwriting)
        std::error_code error;
        fs::rename( out_file + TSTRING( ".tmp" ), out_file, error );
        if ( error ) {
            throw BitException( "Cannot rename temp archive file", error, out_file );
        }
    }
}

void BitOutputArchive::compressTo( std::vector< byte_t >& out_buffer ) {
    if ( !out_buffer.empty() ) {
        throw BitException( kCannotOverwriteBuffer, std::make_error_code( std::errc::invalid_argument ) );
    }

    CMyComPtr< IOutArchive > new_arc = initOutArchive();
    CMyComPtr< IOutStream > out_mem_stream = new CBufferOutStream( out_buffer );
    CMyComPtr< UpdateCallback > update_callback = new UpdateCallback( *this );
    compressOut( new_arc, out_mem_stream, update_callback );
}

void BitOutputArchive::compressTo( std::ostream& out_stream ) {
    CMyComPtr< IOutArchive > new_arc = initOutArchive();
    CMyComPtr< IOutStream > out_std_stream = new CStdOutStream( out_stream );
    CMyComPtr< UpdateCallback > update_callback = new UpdateCallback( *this );
    compressOut( new_arc, out_std_stream, update_callback );
}

void BitOutputArchive::setArchiveProperties( IOutArchive* out_archive ) const {
    ArchiveProperties properties = mArchiveCreator.getArchiveProperties();
    if ( !properties.names.empty() ) {
        CMyComPtr< ISetProperties > set_properties;
        if ( out_archive->QueryInterface( ::IID_ISetProperties,
                                          reinterpret_cast< void** >( &set_properties ) ) != S_OK ) {
            throw BitException( "ISetProperties unsupported", std::make_error_code( std::errc::not_supported ) );
        }
        if ( set_properties->SetProperties( properties.names.data(), properties.values.data(),
                                            static_cast< uint32_t >( properties.names.size() ) ) != S_OK ) {
            throw BitException( "Cannot set properties of the archive",
                                std::make_error_code( std::errc::invalid_argument ) );
        }
    }
}

BitPropVariant BitOutputArchive::getOutputItemProperty( uint32_t index, BitProperty propID ) const {
    auto mapped_index = getItemInputIndex( index );
    return getItemProperty( mapped_index, propID );
}

HRESULT BitOutputArchive::getOutputItemStream( uint32_t index, ISequentialInStream** inStream ) const {
    auto mapped_index = getItemInputIndex( index );
    return getItemStream( mapped_index, inStream );
}

input_index BitOutputArchive::getItemInputIndex( uint32_t new_index ) const {
    auto index = static_cast< decltype( mInputIndices )::size_type >( new_index );
    if ( index < mInputIndices.size() ) {
        return mInputIndices[ index ];
    }
    // if we are here, the user didn't delete any item, so a input_index is essentially equivalent to new_index
    return static_cast< input_index >( new_index );
}

void BitOutputArchive::updateInputIndices() {
    if ( mDeletedItems.empty() ) {
        return;
    }

    uint32_t offset = 0;
    for ( uint32_t new_index = 0; new_index < itemsCount(); ++new_index ) {
        for ( auto it = mDeletedItems.find( new_index + offset );
              it != mDeletedItems.end() && *it == new_index + offset;
              ++it ) {
            ++offset;
        }
        mInputIndices.push_back( static_cast< input_index >( new_index + offset ) );
    }
}

uint32_t BitOutputArchive::itemsCount() const {
    auto result = static_cast< uint32_t >( mNewItemsVector.size() );
    if ( mInputArchive != nullptr ) {
        result += mInputArchive->itemsCount() - static_cast< uint32_t >( mDeletedItems.size() );
    }
    return result;
}

BitPropVariant BitOutputArchive::getItemProperty( input_index index, BitProperty propID ) const {
    auto new_item_index = static_cast< size_t >( index ) - static_cast< size_t >( mInputArchiveItemsCount );
    const GenericItem& new_item = mNewItemsVector[ new_item_index ];
    return new_item.getProperty( propID );
}

HRESULT BitOutputArchive::getItemStream( input_index index, ISequentialInStream** inStream ) const {
    auto new_item_index = static_cast< size_t >( index ) - static_cast< size_t >( mInputArchiveItemsCount );
    const GenericItem& new_item = mNewItemsVector[ new_item_index ];

    HRESULT res = new_item.getStream( inStream );
    if ( FAILED( res ) ) {
        auto path = new_item.path();
        std::error_code ec;
        if ( fs::exists( path, ec ) ) {
            ec = std::make_error_code( std::errc::file_exists );
        }
        mFailedFiles.emplace_back( path.native(), ec );
    }
    return res;
}

bool BitOutputArchive::hasNewData( uint32_t index ) const {
    auto original_index = static_cast< uint32_t >( getItemInputIndex( index ) );
    return original_index >= mInputArchiveItemsCount;
}

bool BitOutputArchive::hasNewProperties( uint32_t index ) const {
    /* Note: in BitOutputArchive, you can only add new items or overwrite (delete + add) existing ones.
     * So if we have new data, we also have new properties! This is not true for BitArchiveEditor! */
    return hasNewData( index );
}

uint32_t BitOutputArchive::getIndexInArchive( uint32_t index ) const {
    auto original_index = static_cast< uint32_t >( getItemInputIndex( index ) );
    return original_index < mInputArchiveItemsCount ? original_index : static_cast< uint32_t >( -1 );
}

const BitArchiveHandler& BitOutputArchive::getHandler() const {
    return mArchiveCreator;
}
