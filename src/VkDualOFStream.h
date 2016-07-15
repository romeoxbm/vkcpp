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
#ifndef VKDUALOFSTREAM_H
#define VKDUALOFSTREAM_H

#include <fstream>
#include "VkCppGenerator.h"

namespace vk
{
	class DualOFStream
	{
	public:
		DualOFStream( const CppGenerator::Options& opt );
		~DualOFStream();

		std::ofstream& hdr() { return *_hdr; }
		std::ofstream& src();

		bool usingDualStream() const { return _src != 0; }

		const std::string& headerFileName() const { return _hdrFileName; }
		const std::string& sourceFileName() const { return _srcFileName; }

		template<typename T>
		DualOFStream& operator<<( const T& value )
		{
			*_hdr << value;

			if( _src )
				*_src << value;

			return *this;
		}

		typedef DualOFStream& ( *DualOFStreamManip )( DualOFStream& );

		DualOFStream& operator<<( DualOFStreamManip manip );

		typedef std::basic_ostream<char, std::char_traits<char>> CoutType;

		// this is the function signature of std::endl
		typedef CoutType& ( *StandardEndLine )( CoutType& );

		// define an operator<< to take in std::endl
		DualOFStream& operator<<( StandardEndLine manip );

	private:
		std::ofstream* _hdr = 0;
		std::ofstream* _src = 0;

		std::string _hdrFileName;
		std::string _srcFileName;
	};
}
#endif // VKDUALOFSTREAM_H
