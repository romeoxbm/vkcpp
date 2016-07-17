// Copyright(c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include "VkTextIndent.h"
#include <iomanip>
#include <ostream>
#include <assert.h>

namespace vk
{
	TextIndent::TextIndent( char indent, unsigned short size )
		: _indentChar( indent ),
		  _size( size )
	{
		assert( size );
	}
	//--------------------------------------------------------------------------
	TextIndent& TextIndent::operator=( unsigned short value )
	{
		_level = value;
		return *this;
	}
	//--------------------------------------------------------------------------
	TextIndent& TextIndent::operator++()
	{
		_level++;
		return *this;
	}
	//--------------------------------------------------------------------------
	TextIndent& TextIndent::operator--()
	{
		if( _level > 0 )
			_level--;

		return *this;
	}
	//--------------------------------------------------------------------------
	TextIndent& TextIndent::operator+=( unsigned short value )
	{
		_level += value;
		return *this;
	}
	//--------------------------------------------------------------------------
	TextIndent& TextIndent::operator-=( unsigned short value )
	{
		if( value < _level )
			_level -= value;
		else
			_level = 0;

		return *this;
	}
	//--------------------------------------------------------------------------
	TextIndent TextIndent::operator+( unsigned short value )
	{
		TextIndent copy( *this );
		copy += value;
		return copy;
	}
	//--------------------------------------------------------------------------
	TextIndent TextIndent::operator-( unsigned short value )
	{
		TextIndent copy( *this );
		copy -= value;
		return copy;
	}
	//--------------------------------------------------------------------------
	std::ostream& operator<<( std::ostream& os, TextIndent const& i )
	{
		os << std::setfill( i._indentChar ) << std::setw( i._level * i._size ) << "";
		return os;
	}
}
