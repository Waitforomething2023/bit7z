#ifndef BITINPUTARCHIVE_H
#define BITINPUTARCHIVE_H

#include "../include/bitarchivehandler.hpp"
#include "../include/bitarchiveiteminfo.hpp"
#include "../include/bitarchiveitemoffset.hpp"
#include "../include/bitformat.hpp"
#include "../include/bitpropvariant.hpp"
#include "../include/bittypes.hpp"

#include <vector>
#include <string>
#include <cstdint>

struct IInStream;
struct IInArchive;
struct IOutArchive;
struct IArchiveExtractCallback;

namespace bit7z {
    using std::vector;

    class ExtractCallback;

    class BitInputArchive {
        public:
            BitInputArchive( const BitArchiveHandler& handler, const tstring& in_file );

            BitInputArchive( const BitArchiveHandler& handler, const vector< byte_t >& in_buffer );

            BitInputArchive( const BitArchiveHandler& handler, std::istream& in_stream );

            virtual ~BitInputArchive();

            /**
             * @return the detected format of the file.
             */
            const BitInFormat& detectedFormat() const;

            /**
             * @brief Gets the specified archive property.
             *
             * @param property  the property to be retrieved.
             *
             * @return the current value of the archive property or an empty BitPropVariant if no value is specified.
             */
            BitPropVariant getArchiveProperty( BitProperty property ) const;

            /**
             * @brief Gets the specified property of an item in the archive.
             *
             * @param index     the index (in the archive) of the item.
             * @param property  the property to be retrieved.
             *
             * @return the current value of the item property or an empty BitPropVariant if the item has no value for
             * the property.
             */
            BitPropVariant getItemProperty( uint32_t index, BitProperty property ) const;

            /**
             * @return the number of items contained in the archive.
             */
            uint32_t itemsCount() const;

            /**
             * @param index the index of an item in the archive.
             *
             * @return true if and only if the item at index is a folder.
             */
            bool isItemFolder( uint32_t index ) const;

            /**
             * @param index the index of an item in the archive.
             *
             * @return true if and only if the item at index is encrypted.
             */
            bool isItemEncrypted( uint32_t index ) const;

        protected:
            IInArchive* openArchiveStream( const BitArchiveHandler& handler, const tstring& name, IInStream* in_stream );
            HRESULT initUpdatableArchive( IOutArchive** newArc ) const;

            void extract( const vector< uint32_t >& indices, ExtractCallback* extract_callback ) const;

            void test( ExtractCallback* extract_callback ) const;

            HRESULT close() const;

            friend class BitArchiveOpener;
            friend class BitExtractor;
            friend class BitMemExtractor;
            friend class BitStreamExtractor;
            friend class BitArchiveCreator;

        private:
            IInArchive* mInArchive;
            const BitInFormat* mDetectedFormat;

        public:
            class const_iterator {
                public:
                    // iterator traits
                    using iterator_category = std::input_iterator_tag;
                    using value_type = BitArchiveItemOffset;
                    using reference = const BitArchiveItemOffset&;
                    using pointer = const BitArchiveItemOffset*;
                    using difference_type = uint32_t; //so that count_if returns a uint32_t

                    const_iterator& operator++();

                    const_iterator operator++( int );

                    bool operator==( const const_iterator& other ) const;

                    bool operator!=( const const_iterator& other ) const;

                    reference operator*();

                    pointer operator->();

                private:
                    BitArchiveItemOffset mItemOffset;

                    const_iterator( uint32_t item_index, const BitInputArchive& item_archive );

                    friend class BitInputArchive;
            };

            const_iterator begin() const NOEXCEPT;

            const_iterator end() const NOEXCEPT;

            const_iterator cbegin() const NOEXCEPT;

            const_iterator cend() const NOEXCEPT;
    };
}

#endif //BITINPUTARCHIVE_H
