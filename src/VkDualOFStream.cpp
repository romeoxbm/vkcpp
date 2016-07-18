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
#include "VkDualOFStream.h"
#include <iostream>

namespace vk
{
	DualOFStream::DualOFStream( const CppGenerator::Options& opt )
	{
		auto lastDirChar = opt.outHeaderDirectory[ opt.outHeaderDirectory.size() - 1 ];
		auto hasDirTrailingSlash = lastDirChar == '\\' || lastDirChar == '/';
		auto sep = !hasDirTrailingSlash ? "/" : "";

		auto he = opt.headerExt[ 0 ] == '.' ? opt.headerExt : "." + opt.headerExt;
		_hdrFileName = opt.outFileName + he;
		auto dest = opt.outHeaderDirectory + sep + _hdrFileName;

		std::cout << "Writing to \"" << _hdrFileName << "\"";
		_hdr = new std::ofstream( dest );

		if( !opt.srcExt.empty() )
		{
			if( !opt.outSrcDirectory.empty() )
			{
				lastDirChar = opt.outSrcDirectory[ opt.outSrcDirectory.size() - 1 ];
				hasDirTrailingSlash = lastDirChar == '\\' || lastDirChar == '/';
				sep = !hasDirTrailingSlash ? "/" : "";
			}

			auto se = opt.srcExt[ 0 ] == '.' ? opt.srcExt : "." + opt.srcExt;
			_srcFileName = opt.outFileName + se;

			dest = !opt.outSrcDirectory.empty() ? opt.outSrcDirectory : opt.outHeaderDirectory;
			dest += sep + _srcFileName;

			std::cout << " and to \"" << _srcFileName << "\"";
			_src = new std::ofstream( dest );
		}

		std::cout << " (" << opt.outHeaderDirectory
				  << ( !opt.outSrcDirectory.empty() ? " , " + opt.outSrcDirectory : "" )
				  << ")\n";
	}
	//--------------------------------------------------------------------------
	DualOFStream::~DualOFStream()
	{
		delete _hdr;
		_hdr = 0;

		if( _src )
		{
			delete _src;
			_src = 0;
		}
	}
	//--------------------------------------------------------------------------
	std::ofstream& DualOFStream::src()
	{
		if( _src )
			return *_src;

		return hdr();
	}
	//--------------------------------------------------------------------------
	DualOFStream& DualOFStream::operator<<( DualOFStreamManip manip )
	{
		return manip( *this );
	}
	//--------------------------------------------------------------------------
	DualOFStream& DualOFStream::operator<<( StandardEndLine manip )
	{
		manip( *_hdr );
		if( _src )
			manip( *_src );

		return *this;
	}
}
