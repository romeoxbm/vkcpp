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
#include <iostream>

#include "VkVersion.h"
#include "cmdline.h"
#include "VkCppGenerator.h"

int main( int argc, char** argv )
{
	cmdline::parser cmd;
	cmd.set_program_name( "VkCppGenerator" );
	cmd.footer( "<spec file>" );

	//Define command line options
	cmd.add( "cmdline", 'c', "Add a comment in the generated files containing the command line options used" );
	cmd.add<std::string>( "directory", 'd', "Change the default output directory. Default value is", false, "./" );
	cmd.add<std::string>( "headerext", 'e', "Change the default header extension. Default value is", false, ".hpp" );
	cmd.add<std::string>( "filename", 'f', "Change the default file name (DO NOT specify file extension). Default value is", false, "vk_cpp" );
	cmd.add<std::string>( "guard", 'g', "Change the include guard. Default value is", false, "VK_CPP_H_" );
	cmd.add( "spaceindent", 'i', "Use spaces to indent generated files. By default, it uses tabs." );
	cmd.add<unsigned short>( "spacesize", 'z', "Specify spaces size. Only used when indenting with spaces", false, 2 );
	cmd.add<std::string>( "pch", 'p', "Specify a precompiled header to include in source file (Used if generating separate files). Default value is", false );
	cmd.add( "separate", 'r', "Generate separate header and source files." );
	cmd.add<std::string>( "srcext", 's', "Change the default source extension (Used if generating separate files). Default value is", false, ".cc" );
	cmd.add( "version", 'v', "Print version and exit" );
	cmd.add( "help", 'h', "Print this message and exit" );
	cmd.parse_check( argc, argv );

	if( cmd.exist( "version" ) )
	{
		std::cout << "VkCppGenerator v" << VERSION << std::endl;
		return 0;
	}

	if( cmd.rest().empty() || cmd.rest().size() > 1 )
	{
		std::cerr << cmd.usage();
		return -1;
	}

	vk::CppGenerator vGen;
	vk::CppGenerator::Options opt;

	opt.inputFile = cmd.rest()[ 0 ];
	opt.outFileName = cmd.get<std::string>( "filename" );
	opt.outDirectory = cmd.get<std::string>( "directory" );
	opt.headerExt = cmd.get<std::string>( "headerext" );
	opt.includeGuard = cmd.get<std::string>( "guard" );

	if( cmd.exist( "separate" ) )
	{
		opt.srcExt = cmd.get<std::string>( "srcext" );
		opt.pch = cmd.get<std::string>( "pch" );
	}

	if( cmd.exist( "cmdline" ) )
	{
		std::stringstream ss;
		for( int i = 1; i < argc - 1; i++ )
			ss << argv[ i ] << " ";

		ss << argv[ argc - 1 ];
		opt.cmdLine = ss.str();
	}

	if( cmd.exist( "spaceindent" ) )
	{
		opt.indentChar = ' ';
		opt.spaceSize = cmd.get<unsigned short>( "spacesize" );
	}
	else
		opt.indentChar = '\t';

	return vGen.generate( opt );
}
