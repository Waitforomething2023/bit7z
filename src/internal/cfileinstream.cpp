// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/*
 * bit7z - A C++ static library to interface with the 7-zip shared libraries.
 * Copyright (c) 2014-2022 Riccardo Ostani - All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "internal/cfileinstream.hpp"

#include "bitexception.hpp"

using namespace bit7z;

CFileInStream::CFileInStream( const fs::path& filePath ) : CStdInStream( mFileStream ), mBuffer{} {
    open( filePath );

    /* By default, file stream performance is relatively poor due to the default buffer size used
     * (e.g., GCC uses a small 1024 bytes buffer).
     * This is a known problem (see https://stackoverflow.com/questions/26095160/why-are-stdfstreams-so-slow).
     * We make the underlying file stream use a bigger buffer (1 MiB) for optimizing the reading of big files.  */
    mFileStream.rdbuf()->pubsetbuf( mBuffer.data(), buffer_size );
}

void CFileInStream::open( const fs::path& filePath ) {
    mFileStream.open( filePath, std::ios::in | std::ios::binary );
    if ( mFileStream.fail() ) {
        //Note: CFileInStream constructor does not directly throw exceptions since it is also used in nothrow functions.
        throw BitException( "Failed to open the archive file",
                            make_hresult_code( HRESULT_FROM_WIN32( ERROR_OPEN_FAILED ) ),
                            filePath.string< tchar >() );
    }
}
