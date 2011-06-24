/*
Copyright (C) 2009 Electronic Arts, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/////////////////////////////////////////////////////////////////////////////
// EAStreamAdapter.h
//
// Copyright (c) 2003, Electronic Arts Inc. All rights reserved.
// Written by Paul Pedriana
//
// Implements adapters for reading and writing basic types in an 
// endian-proper way with the EA::Stream interfaces.
//
/////////////////////////////////////////////////////////////////////////////


#if !defined(EAIO_EASTREAMADAPTER_H) && !defined(FOUNDATION_EASTREAMADAPTER_H)
#define EAIO_EASTREAMADAPTER_H
#define FOUNDATION_EASTREAMADAPTER_H    // For backward compatability. Eventually, we'll want to get rid of this eventually.


#ifndef INCLUDED_eabase_H
    #include <EABase/eabase.h>
#endif
#include <EAIO/internal/Config.h>
#ifndef EAIO_EASTREAM_H
    #include <EAIO/EAStream.h>
#endif
#include <string.h>
#include <limits.h>



/// namespace EA
/// Standard Electronic Arts Framework namespace.
namespace EA
{
    /// namespace IO
    /// The namespace for general IO (input/output) functionality.
    namespace IO
    {
        /// CopyStream
        ///
        /// This is a generic function for copying a source stream to a destination stream.
        ///
        /// If input nSize is less than the input stream size, then nSize bytes are copied.
        /// If input nSize is greater than the input stream size, then only the input stream
        /// size is copied. If input nSize is kLengthNull, then the entire input stream size 
        /// should be copied, whatever it happens to be.
        ///
        /// Returns kSizeTypeError if there was an error reading input or writing output. 
        /// Upon error, the input and output stream states are left as they were upon reaching 
        /// the error. The reading from source begins at the current source position, which 
        /// may be different from the beginning of the stream. The writing of the destination 
        /// begins at the current destination position, which may be different from the beginning 
        /// of the stream. In some cases, a given source and/or destination stream might be 
        /// specialized to outperform this function. 
        ///
        size_type EAIO_API CopyStream(IStream* pSource, IStream* pDestination, size_type nSize = kLengthNull);



        ///////////////////////////////////////////////////////////////////
        // Adapter functions for reading data from streams
        //
        // Example usage:
        //    IStream* pIStream = GetStreamFromSomewhere();
        //
        //    uint16_t nUint16;
        //    ReadUint16(pIStream, nUint16);
        //
        //    uint32_t nUint32(0);
        //    WriteUint32(pIStream, nUint32);
        //
        //    const char16_t line[] = L"HelloWorld";
        //    ReadLine(pIStream, line, sizeof(line), kEndianBig);
        //
        //    WriteLine(pIStream, "Hello World", kLengthNull, kLineTerminationNewline);
        ///////////////////////////////////////////////////////////////////

        /// ReadBool8
        ///
        /// Reads a single boolean value from the stream. Note that the to be 
        /// portable, the stream implments booleans as int8_t and not bool.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadBool8(IStream* pIS, bool& value);

        /// ReadInt8
        ///
        /// Reads an int8_t from the stream.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt8(IStream* pIS, int8_t& value);

        /// ReadInt8
        ///
        /// Reads an array of int8_t from the stream.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt8(IStream* pIS, int8_t* value, size_type count);

        /// ReadUint8
        ///
        /// Reads an uint8_t from the stream.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint8(IStream* pIS, uint8_t& value);

        /// ReadUint8
        ///
        /// Reads an array of uint8_t from the stream.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint8(IStream* pIS, uint8_t* value, size_type count);

        /// ReadUint16
        ///
        /// Reads a uint16_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint16(IStream* pIS, uint16_t& value, Endian endianSource = kEndianBig);

        /// ReadUint16
        ///
        /// Reads an array of uint16_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint16(IStream* pIS, uint16_t* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadInt16
        ///
        /// Reads an int16_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt16(IStream* pIS, int16_t& value, Endian endianSource = kEndianBig);

        /// ReadInt16
        ///
        /// Reads an array of int16_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt16(IStream* pIS, int16_t* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadUint32
        ///
        /// Reads a uint32_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint32(IStream* pIS, uint32_t& value, Endian endianSource = kEndianBig);

        /// ReadUint32
        ///
        /// Reads an array of uint32_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint32(IStream* pIS, uint32_t* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadInt32
        ///
        /// Reads an int32_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt32(IStream* pIS, int32_t& value, Endian endianSource = kEndianBig);

        /// ReadInt32
        ///
        /// Reads an array of int32_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt32(IStream* pIS, int32_t* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadUint64
        ///
        /// Reads a uint64_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint64(IStream* pIS, uint64_t& value, Endian endianSource = kEndianBig);

        /// ReadUint64
        ///
        /// Reads an array of uint64_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadUint64(IStream* pIS, uint64_t* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadInt64
        ///
        /// Reads an int64_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt64(IStream* pIS, int64_t& value, Endian endianSource = kEndianBig);

        /// ReadInt64
        ///
        /// Reads an array of int64_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadInt64(IStream* pIS, int64_t* value, size_type count, Endian endianSource = kEndianBig);

        // ReadUint128
        // Reads a uint128_t from the stream, converting to local endian-ness as directed by input endianSource.
        // Input 'endianSource' refers to the endian-ness of the values in the stream.
        // Upon return the output value will be in local (a.k.a. native) endian-ness.
        // The return value is true if the value could be entirely read. 
        // If false, you can use IStream::GetState to determine the error.
        // bool EAIO_API ReadUint128(IStream* pIS, uint128_t& value, Endian endianSource = kEndianBig);

        // ReadUint128
        // Reads an array of uint128_t from the stream.
        // Input 'endianSource' refers to the endian-ness of the values in the stream.
        // Upon return the output value will be in local (a.k.a. native) endian-ness.
        // The return value is true if the value could be entirely read. 
        // If false, you can use IStream::GetState to determine the error.
        // bool EAIO_API ReadUint128(IStream* pIS, uint128_t* value, size_type count, Endian endianSource = kEndianBig);

        // ReadInt128
        // Reads an int128_t from the stream.
        // Input 'endianSource' refers to the endian-ness of the values in the stream.
        // Upon return the output value will be in local (a.k.a. native) endian-ness.
        // The return value is true if the value could be entirely read. 
        // If false, you can use IStream::GetState to determine the error.
        // bool EAIO_API ReadInt128(IStream* pIS, int128_t& value, Endian endianSource = kEndianBig);

        // ReadInt128
        // Reads an array of int128_t from the stream.
        // Input 'endianSource' refers to the endian-ness of the values in the stream.
        // Upon return the output value will be in local (a.k.a. native) endian-ness.
        // The return value is true if the value could be entirely read. 
        // If false, you can use IStream::GetState to determine the error.
        // bool EAIO_API ReadInt128(IStream* pIS, int128_t* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadFloat
        ///
        /// Reads a float from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadFloat(IStream* pIS, float& value, Endian endianSource = kEndianBig);

        /// ReadFloat
        ///
        /// Reads an array of float from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadFloat(IStream* pIS, float* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadDouble
        ///
        /// Reads a double from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadDouble(IStream* pIS, double& value, Endian endianSource = kEndianBig);

        /// ReadDouble
        ///
        /// Reads an array of double_t from the stream.
        /// Input 'endianSource' refers to the endian-ness of the values in the stream.
        /// Upon return the output value will be in local (a.k.a. native) endian-ness.
        /// The return value is true if the value could be entirely read. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API ReadDouble(IStream* pIS, double* value, size_type count, Endian endianSource = kEndianBig);

        /// ReadString
        ///
        /// Reads a string from an IStream.
        ///
        /// The resulting buffer will be nul-terminated, even if there wasn't enough 
        /// space to write the full string (with the exception of nStringCapacity == 0).
        ///
        /// nStringCapacity is the capacity of the pString buffer; it is the max number 
        /// of characters to write, including the terminating nul. The input pLine can 
        /// be NULL, in which case it will not be written to and the return value will 
        /// merely indicate its required size.
        ///
        /// The return value is the strlen (string length) of the expected line or 
        /// kSizeTypeError if there was an error. 
        /// 
        /// Upon return, the stream will be positioned after the end of the string, 
        /// even if nStringCapacity was not enough to hold the entire string.
        ///
        size_type EAIO_API ReadString(IStream* pIS, char8_t* pString, size_type nStringCapacity, Endian endianSource = kEndianBig);

        /// ReadString
        ///
        /// This is a char16_t version of ReadString.
        /// It behaves the same as with the char8_t version with the exception that
        /// the destination output is written as UTF16-encoded char16_t.
        ///
        size_type EAIO_API ReadString(IStream* pIS, char16_t* pString, size_type nStringCapacity, Endian endianSource = kEndianBig);

        /// ReadLine
        ///
        /// Reads a line of text from the source IStream.
        ///
        /// A line is defined as a sequence ending with "\n" or "\r\n".
        /// A line may be empty, as would be the case with a "\n\n" sequence.
        /// The returned line does not end with line terminator characters.
        /// The returned line -does- end with a terminating zero.
        ///
        /// The return value is the strlen (string length) of the expected line or 
        /// kSizeTypeError upon error. A return value of kSizeTypeDone means there 
        /// were no more lines to read. This is different from the return value of 
        /// the IStream::Read function because of the possibility of empty lines.
        /// Note that the return value is the *expected* strlen, which may be >=
        /// the nLineCapacity. In any case, the returned line will always be 
        /// nul-terminated if it has any capacity.
        ///
        /// Upon return, the stream will be positioned at the beginning of the 
        /// next line, even if input nLineCapacity was not enough to hold the entire line.
        /// The input nLineCapacity is max number of characters to write, including 
        /// the terminating zero. The input pLine can be NULL, in which case it 
        /// will not be written to and the return value will merely indicate its 
        /// required size.
        ///
        /// Example usage:
        ///     char      buffer[256];
        ///     size_type size;
        ///     while((size = EA::IO::ReadLine(&fileStream, buffer, 256)) < kSizeTypeDone) // While there there were more lines...
        ///         ; // do something
        ///
        size_type EAIO_API ReadLine(IStream* pIS, char8_t* pLine, size_type nLineCapacity);

        /// ReadLine
        ///
        /// This is a char16_t version of ReadLine.
        /// It behaves the same as with the char8_t version with the exception that
        /// the destination output is written as UTF16-encoded char16_t.
        ///
        size_type EAIO_API ReadLine(IStream* pIS, char16_t* pLine, size_type nLineCapacity, Endian endianSource = kEndianBig);




        /// WriteUint8
        ///
        /// Writes a boolean value to the output stream.
        /// Since type 'bool' is not portable, this function converts the input to 
        /// an int8_t and write that as 0 or 1.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteBool8(IStream* pOS, bool value);

        /// WriteInt8
        ///
        /// Writes an int8_t numerical value to the output stream.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt8(IStream* pOS, int8_t value);

        /// WriteInt8
        ///
        /// Writes an array of int8_t numerical values to the output stream.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt8(IStream* pOS, const int8_t *value, size_type count);

        /// WriteUint8
        ///
        /// Writes a uint8_t numerical value to the output stream.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint8(IStream* pOS, uint8_t value);

        /// WriteUint8
        ///
        /// Writes an array of uint8_t numerical values to the output stream.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint8(IStream* pOS, const uint8_t *value, size_type count);

        /// WriteUint16
        ///
        /// Writes a uint16_t numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint16(IStream* pOS, uint16_t value, Endian endianDestination = kEndianBig);

        /// WriteUint16
        ///
        /// Writes an array of uint16_t numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint16(IStream* pOS, const uint16_t *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteInt16
        ///
        /// Writes an int16_t numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt16(IStream* pOS, int16_t value, Endian endianDestination = kEndianBig);

        /// WriteInt16
        ///
        /// Writes an array of int16_t numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt16(IStream* pOS, const int16_t *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteUint32
        ///
        /// Writes a uint32_t numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint32(IStream* pOS, uint32_t value, Endian endianDestination = kEndianBig);

        /// WriteUint32
        ///
        /// Writes an array of uint32_t numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint32(IStream* pOS, const uint32_t *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteInt32
        ///
        /// Writes an int32_t numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt32(IStream* pOS, int32_t value, Endian endianDestination = kEndianBig);

        /// WriteInt32
        ///
        /// Writes an array of int32_t numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt32(IStream* pOS, const int32_t *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteUint64
        ///
        /// Writes a uint64_t numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint64(IStream* pOS, uint64_t value, Endian endianDestination = kEndianBig);

        /// WriteUint64
        ///
        /// Writes an array of uint64_t numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteUint64(IStream* pOS, const uint64_t *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteInt64
        ///
        /// Writes an int64_t numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt64(IStream* pOS, int64_t value, Endian endianDestination = kEndianBig);

        /// WriteInt64
        ///
        /// Writes an array of int64_t numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteInt64(IStream* pOS, const int64_t *value, size_type count, Endian endianDestination = kEndianBig);

        // WriteUint128
        //
        // Writes a uint128_t numerical value to the output stream with the given destination endian-ness.
        // The return value is true if the value could be successfully completely written. 
        // If false, you can use IStream::GetState to determine the error.
        //
        // bool EAIO_API WriteUint128(IStream* pOS, uint128_t value, Endian endianDestination = kEndianBig);

        // WriteUint128
        // Writes an array of uint128_t numerical values to the output stream with the given destination endian-ness.
        // The return value is true if the value could be successfully completely written. 
        // If false, you can use IStream::GetState to determine the error.
        // bool WriteUint128(IStream* pOS, const uint128_t *value, size_type count, Endian endianDestination = kEndianBig);

        // WriteInt128
        //
        // Writes an int128_t numerical value to the output stream with the given destination endian-ness.
        // The return value is true if the value could be successfully completely written. 
        // If false, you can use IStream::GetState to determine the error.
        //
        // bool EAIO_API WriteInt128(IStream* pOS, int128_t value, Endian endianDestination = kEndianBig);

        // WriteInt128
        //
        // Writes an array of int128_t numerical values to the output stream with the given destination endian-ness.
        // The return value is true if the value could be successfully completely written. 
        // If false, you can use IStream::GetState to determine the error.
        //
        // bool EAIO_API WriteInt128(IStream* pOS, const int128_t *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteFloat
        ///
        /// Writes a float numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteFloat(IStream* pOS, float value, Endian endianDestination = kEndianBig);

        /// WriteFloat
        ///
        /// Writes an array of float numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteFloat(IStream* pOS, const float *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteDouble
        ///
        /// Writes a double numerical value to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteDouble(IStream* pOS, double_t value, Endian endianDestination = kEndianBig);

        /// WriteDouble
        ///
        /// Writes an array of double_t numerical values to the output stream with the given destination endian-ness.
        /// The return value is true if the value could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteDouble(IStream* pOS, const double_t *value, size_type count, Endian endianDestination = kEndianBig);

        /// WriteString
        ///
        /// Writes a string to an IStream.
        ///
        /// The source line is not expected to end with a nul-terminator; if it has such 
        /// a terminator, the nSourceLineLength value should not include that terminator.
        /// If input nSourceLineLength is kLengthNull, then the line is expected to be 
        /// nul-terminated and the length written is the strlen of pSourceLine.
        /// The pSourceLine value must be non-NULL unless nSourceLineLength is zero.
        ///
        /// The return value is true if the string could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error.
        ///
        bool EAIO_API WriteString(IStream* pOS, const char8_t* pString, size_t nStringLength, Endian endianDestination = kEndianBig);

        /// WriteString
        ///
        /// This is the char16_t version of the WriteString function. 
        /// It behaves the same as with the char8_t version with the exception that
        /// the source is written as UTF16-encoded char16_t.
        ///
        bool EAIO_API WriteString(IStream* pOS, const char16_t* pString, size_t nStringLength, Endian endianDestination = kEndianBig);

        /// WriteLine
        ///
        /// Writes a line of text to a destination IStream.
        ///
        /// The source line is not expected to end in a line delimiter (e.g. '\n'); 
        /// a appropriate line delimiter (defined by the lineEndToUse parameter) will be 
        /// written to the stream by this function. The source line is not expected to end 
        /// with a nul-terminator; if it has such a terminator, the nSourceLineLength value 
        /// should not include that terminator.
        ///
        /// If input nLineLength is kLengthNull, then the line is expected to be 
        /// nul-terminated and the length written is the strlen of pSourceLine.
        /// The pLineSource value must be non-NULL unless nLineLength is zero.
        ///
        /// The return value is true if the line could be successfully completely written. 
        /// If false, you can use IStream::GetState to determine the error, as this function
        /// generates no errors beyond those related to IStream errors.
        ///
        bool EAIO_API WriteLine(IStream* pOS, const char8_t* pLineSource, size_type nLineLength, LineEnd lineEndToUse = kLineEndAuto);

        /// WriteLine
        ///
        /// This is the char16_t version of the WriteLine function. 
        /// It behaves the same as with the char8_t version with the exception that
        /// the source is written as UTF16-encoded char16_t.
        ///
        bool EAIO_API WriteLine(IStream* pOS, const char16_t* pLineSource, size_type nLineLength, LineEnd lineEndToUse = kLineEndAuto, Endian endianDestination = kEndianBig);

    } // namespace IO

} // namespace EA



/// \name C++-style stream wrappers.
/// \note Only intended for print output.
//@{
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, const char16_t* str) { EA::IO::WriteLine(&s, str, EA::IO::kLengthNull, EA::IO::kLineEndNone); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, const char8_t* str)  { EA::IO::WriteLine(&s, str, EA::IO::kLengthNull, EA::IO::kLineEndNone); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, int64_t val)         { EA::IO::WriteInt64 (&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, uint64_t val)        { EA::IO::WriteUint64(&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, int32_t val)         { EA::IO::WriteInt32 (&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, uint32_t val)        { EA::IO::WriteUint32(&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, int16_t val)         { EA::IO::WriteInt16 (&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, uint16_t val)        { EA::IO::WriteUint16(&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, int8_t val)          { EA::IO::WriteInt8  (&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, uint8_t val)         { EA::IO::WriteUint8 (&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, bool val)            { EA::IO::WriteBool8 (&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, float val)           { EA::IO::WriteFloat (&s, val); return s; }
inline EA::IO::IStream& operator<<(EA::IO::IStream& s, double val)          { EA::IO::WriteDouble(&s, val); return s; }

inline EA::IO::IStream& operator>>(EA::IO::IStream& s, int64_t&  val)       { EA::IO::ReadInt64 (&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, uint64_t& val)       { EA::IO::ReadUint64(&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, int32_t&  val)       { EA::IO::ReadInt32 (&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, uint32_t& val)       { EA::IO::ReadUint32(&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, int16_t&  val)       { EA::IO::ReadInt16 (&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, uint16_t& val)       { EA::IO::ReadUint16(&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, int8_t&    val)      { EA::IO::ReadInt8  (&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, uint8_t&  val)       { EA::IO::ReadUint8 (&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, bool&      val)      { EA::IO::ReadBool8 (&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, float&  val)         { EA::IO::ReadFloat (&s, val); return s; }
inline EA::IO::IStream& operator>>(EA::IO::IStream& s, double& val)         { EA::IO::ReadDouble(&s, val); return s; }
//@}



#endif // Header include guard







