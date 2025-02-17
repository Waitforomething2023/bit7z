/*
 * bit7z - A C++ static library to interface with the 7-zip shared libraries.
 * Copyright (c) 2014-2022 Riccardo Ostani - All Rights Reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef MACROS_HPP
#define MACROS_HPP

//p7zip defines IUnknown with a virtual destructor, while Windows' IUnknown has a non-virtual destructor
#ifdef _WIN32
#define MY_UNKNOWN_DESTRUCTOR( x ) x
#else
#define MY_UNKNOWN_DESTRUCTOR( x ) x override
#endif

// Some stream classes are non-final (e.g., CStdOutStream), so on Windows they must have a virtual destructor
#ifdef _WIN32
#define MY_UNKNOWN_VIRTUAL_DESTRUCTOR( x ) virtual x
#else
#define MY_UNKNOWN_VIRTUAL_DESTRUCTOR( x ) MY_UNKNOWN_DESTRUCTOR(x)
#endif

#ifndef _WIN32
#define COM_DECLSPEC_NOTHROW
#endif

#define MY_STDMETHOD( method, ... ) HRESULT STDMETHODCALLTYPE method ( __VA_ARGS__ )
#define BIT7Z_STDMETHOD( method, ... ) COM_DECLSPEC_NOTHROW MY_STDMETHOD(method, __VA_ARGS__) override
#define BIT7Z_STDMETHOD_NOEXCEPT( method, ... ) MY_STDMETHOD(method, __VA_ARGS__) noexcept override

#endif //MACROS_HPP
