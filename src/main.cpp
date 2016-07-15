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
	cmd.add( "cmdline", 'c', "Add a comment in the generated files containing the command line options used" );
	cmd.add<std::string>( "directory", 'd', "Change the default output directory. Default is", false, "./" );
	cmd.add<std::string>( "headerext", 'e', "Change the default header extension. Default is", false, ".hpp" );
	cmd.add<std::string>( "filename", 'f', "Change the default file name (DO NOT specify file extension). Default is", false, "vk_cpp" );
	cmd.add<std::string>( "guard", 'g', "Change the include guard. Default is", false, "VK_CPP_H_" );
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

	if( cmd.exist( "cmdline" ) )
	{
		std::stringstream ss;
		for( int i = 1; i < argc - 1; i++ )
			ss << argv[ i ] << " ";

		ss << argv[ argc - 1 ];
		opt.cmdLine = ss.str();
	}

	return vGen.generate( opt );
}
