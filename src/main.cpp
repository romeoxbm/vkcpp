#include <iostream>

#include "VkVersion.h"
#include "cmdline.h"
#include "VkCppGenerator.h"

int main( int argc, char** argv )
{
	cmdline::parser cmd;
	cmd.set_program_name( "VkCppGenerator" );
	cmd.footer( "<spec file> ..." );

	//Define command line options
	cmd.add<std::string>( "guard", 'g', "Change the include guard. Default is", false, "VK_CPP_H_" );
	cmd.add<std::string>( "fileName", 'f', "Change the default file name (DO NOT specify file extension). Default is", false, "vk_cpp" );
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
	opt.outFileName = cmd.get<std::string>( "fileName" );
	opt.includeGuard = cmd.get<std::string>( "guard" );

	return vGen.generate( opt );
}
