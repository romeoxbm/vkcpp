#include "VkCppGenerator.h"

int main( int argc, char **argv )
{
	vk::CppGenerator vGen;
	vk::CppGenerator::Options opt;
	opt.inputFile = ( argc == 1 ) ? VK_SPEC : argv[ 1 ];
	return vGen.generate( opt );
}
