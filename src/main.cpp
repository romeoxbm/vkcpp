#include "VkCppGenerator.h"

int main( int argc, char **argv )
{
	std::string filename = ( argc == 1 ) ? VK_SPEC : argv[ 1 ];
	vk::CppGenerator vGen;
	return vGen.generate( filename );
}
