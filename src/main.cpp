#include <iostream>
#include "cmdline.h"
#include "VkCppGenerator.h"

int main( int argc, char** argv )
{
	cmdline::parser cmd;
	cmd.set_program_name( "VkCppGenerator" );
	cmd.footer( "<spec file> ..." );

	//Define command line options
	cmd.add<std::string>( "guard", 'g', "Change the include guard. Default is", false, "VK_CPP_H_" );
	cmd.add( "help", 'h', "Print this message" );
	cmd.parse_check( argc, argv );

	if( cmd.rest().empty() || cmd.rest().size() > 1 )
	{
		std::cerr << cmd.usage();
		return -1;
	}

	vk::CppGenerator vGen;
	vk::CppGenerator::Options opt;

	opt.inputFile =  cmd.rest()[ 0 ]; //( argc == 1 ) ? VK_SPEC : argv[ 1 ];
	opt.includeGuard = cmd.get<std::string>( "guard" );

	return vGen.generate( opt );
}
