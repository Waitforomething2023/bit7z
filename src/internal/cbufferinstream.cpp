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
#ifdef _MSC_VER
// Disable warning
//    C4996: '...': Function call with parameters that may be unsafe
// This is due to the call to std::copy_n with a raw buffer pointer as destination.
#pragma warning(disable:4996)
#endif

#include "internal/cbufferinstream.hpp"
#include "internal/bufferutil.hpp"

#include <algorithm> //for std::copy_n
#include <cstdint>

using namespace bit7z;

CBufferInStream::CBufferInStream( const vector< byte_t >& in_buffer )
    : mBuffer( in_buffer ), mCurrentPosition{ mBuffer.begin() } {}

COM_DECLSPEC_NOTHROW
STDMETHODIMP CBufferInStream::Read( void* data, UInt32 size, UInt32* processedSize ) {
    if ( processedSize != nullptr ) {
        *processedSize = 0;
    }

    if ( size == 0 || mCurrentPosition == mBuffer.cend() ) {
        return S_OK;
    }

    /* Note: thanks to CBufferInStream::Seek, we can safely assume mCurrentPosition to always be a valid iterator;
     * so "remaining" will always be > 0 (and casts to unsigned types are safe) */
    size_t remaining = mBuffer.cend() - mCurrentPosition;
    if ( remaining > static_cast< size_t >( size ) ) {
        /* Remaining buffer still to read is bigger than the buffer size requested by the user,
         * so we need to read just "size" number of bytes. */
        remaining = static_cast< size_t >( size );
    }
    /* else, the user requested to read a number of bytes greater than or equal to the number
     * of remaining bytes to be read from the buffer.
     * So we just read all the remaining bytes, not more or less. */

    /* Note: here remaining is > 0 */
    std::copy_n( mCurrentPosition, remaining, static_cast< byte_t* >( data ) );
    std::advance( mCurrentPosition, remaining );

    if ( processedSize != nullptr ) {
        /* Note: even though on 64-bit systems "remaining" will be a 64-bit unsigned integer (size_t),
         * its value cannot be greater than "size", which is a 32-bit unsigned int. Hence, this cast is safe! */
        *processedSize = static_cast< UInt32 >( remaining );
    }
    return S_OK;
}

COM_DECLSPEC_NOTHROW
STDMETHODIMP CBufferInStream::Seek( Int64 offset, UInt32 seekOrigin, UInt64* newPosition ) noexcept {
    int64_t new_index{};
    const HRESULT res = seek( mBuffer, mCurrentPosition, offset, seekOrigin, new_index );

    if ( res != S_OK ) {
        // new_index is not in the range [0, mBuffer.size]
        return res;
    }

    // Note: new_index can be equal to mBuffer.size(); in this case, mCurrentPosition == mBuffer.cend()
    mCurrentPosition = mBuffer.cbegin() + static_cast< index_t >( new_index );

    if ( newPosition != nullptr ) {
        // Safe cast, since new_index >= 0
        *newPosition = static_cast< UInt64 >( new_index );
    }

    return S_OK;
}
