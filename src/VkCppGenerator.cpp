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
#include "VkCppGenerator.h"
#include "Strings.h"

namespace vk
{
	int CppGenerator::generate( const Options& opt )
	{
		try
		{
			tinyxml2::XMLDocument doc;
			std::cout << "Loading vk.xml from " << opt.inputFile << std::endl;
			std::cout << "Writing vk_cpp.hpp to " << VK_CPP << std::endl;

			tinyxml2::XMLError error = doc.LoadFile( opt.inputFile.c_str() );
			if( error != tinyxml2::XML_SUCCESS )
			{
				std::cerr << "VkGenerate: failed to load file " << opt.inputFile
						  << " . Error code: " << error << std::endl;
				return -1;
			}

			tinyxml2::XMLElement* registryElement = doc.FirstChildElement();
			assert( strcmp( registryElement->Value(), "registry" ) == 0 );
			assert( !registryElement->NextSiblingElement() );

			VkData vkData;

			tinyxml2::XMLElement* child = registryElement->FirstChildElement();
			do
			{
				assert( child->Value() );
				const std::string value = child->Value();
				if( value == "commands" )
					readCommands( child, vkData );

				else if( value == "comment" )
					readComment( child, vkData.vulkanLicenseHeader );

				else if( value == "enums" )
					readEnums( child, vkData );

				else if( value == "extensions" )
					readExtensions( child, vkData );

				else if( value == "tags" )
					readTags( child, vkData.tags );

				else if( value == "types" )
					readTypes( child, vkData );
				else
					assert( ( value == "feature" ) || ( value == "vendorids" ) );
			} while( child = child->NextSiblingElement() );

			sortDependencies( vkData.dependencies );

			std::map<std::string, std::string> defaultValues;
			createDefaults( vkData, defaultValues );

			std::ofstream ofs( VK_CPP );
			ofs << nvidiaLicenseHeader << std::endl
				<< vkData.vulkanLicenseHeader << std::endl
				<< std::endl
				<< std::endl
				<< "#ifndef " << opt.includeGuard << std::endl
				<< "#define " << opt.includeGuard << std::endl
				<< std::endl
				<< "#include <array>" << std::endl
				<< "#include <cassert>" << std::endl
				<< "#include <cstdint>" << std::endl
				<< "#include <cstring>" << std::endl
				<< "#include <string>" << std::endl
				<< "#include <system_error>" << std::endl
				<< "#include <vulkan/vulkan.h>" << std::endl
				<< "#ifndef VKCPP_DISABLE_ENHANCED_MODE" << std::endl
				<< "# include <vector>" << std::endl
				<< "#endif /*VKCPP_DISABLE_ENHANCED_MODE*/" << std::endl
				<< std::endl;

			writeVersionCheck( ofs, vkData.version );
			writeTypesafeCheck( ofs, vkData.typesafeCheck );
			ofs << versionCheckHeader
				<< "namespace vk" << std::endl
				<< "{" << std::endl
				<< flagsHeader
				<< optionalClassHeader
				<< arrayProxyHeader;

			// first of all, write out vk::Result and the exception handling stuff
			std::list<DependencyData>::const_iterator it = std::find_if( vkData.dependencies.begin(), vkData.dependencies.end(), []( DependencyData const& dp ) { return dp.name == "Result"; } );
			assert( it != vkData.dependencies.end() );
			writeTypeEnum( ofs, *it, vkData.enums.find( it->name )->second );
			writeEnumsToString( ofs, *it, vkData.enums.find( it->name )->second );
			vkData.dependencies.erase( it );
			ofs << exceptionHeader;

			ofs << "} // namespace vk" << std::endl
				<< std::endl
				<< isErrorCode
				<< std::endl
				<< "namespace vk" << std::endl
				<< "{" << std::endl
				<< resultValueHeader
				<< createResultValueHeader;

			writeTypes( ofs, vkData, defaultValues );
			writeEnumsToString( ofs, vkData );

			ofs << "} // namespace vk" << std::endl
				<< std::endl
				<< "#endif // " << opt.includeGuard << std::endl;
		}
		catch( const std::exception& e )
		{
			std::cerr << "caught exception: " << e.what() << std::endl;
			return -1;
		}
		catch( ... )
		{
			std::cerr << "caught unknown exception" << std::endl;
			return -1;
		}

		return 0;
	}

	// trim from end
	std::string CppGenerator::trimEnd( std::string const& input )
	{
		std::string result = input;
		result.erase( std::find_if( result.rbegin(), result.rend(), std::not1( std::ptr_fun<int, int>( std::isspace ) ) ).base(), result.end() );
		return result;
	}

	void CppGenerator::EnumData::addEnum( std::string const & name, std::string const& tag, bool appendTag )
	{
		assert( tag.empty() || ( name.find( tag ) != std::string::npos ) );
		members.push_back( NameValue() );
		members.back().name = "e" + toCamelCase( strip( name, prefix, tag ) );
		members.back().value = name;
		if( !postfix.empty() )
		{
			size_t pos = members.back().name.find( postfix );
			if( pos != std::string::npos )
			{
				members.back().name.erase( pos );
			}
		}
		if( appendTag && !tag.empty() )
		{
			members.back().name += tag;
		}
	}

	void CppGenerator::createDefaults( VkData const& vkData, std::map<std::string, std::string> & defaultValues )
	{
		for( std::list<DependencyData>::const_iterator it = vkData.dependencies.begin(); it != vkData.dependencies.end(); ++it )
		{
			assert( defaultValues.find( it->name ) == defaultValues.end() );
			switch( it->category )
			{
				case DependencyData::Category::COMMAND:    // commands should never be asked for defaults
					break;
				case DependencyData::Category::ENUM:
				{
					assert( vkData.enums.find( it->name ) != vkData.enums.end() );
					EnumData const & enumData = vkData.enums.find( it->name )->second;
					if( !enumData.members.empty() )
					{
						defaultValues[ it->name ] = it->name + "::" + vkData.enums.find( it->name )->second.members.front().name;
					}
					else
					{
						defaultValues[ it->name ] = it->name + "()";
					}
				}
				break;
				case DependencyData::Category::FLAGS:
				case DependencyData::Category::HANDLE:
				case DependencyData::Category::STRUCT:
				case DependencyData::Category::UNION:        // just call the default constructor for flags, structs, and structs (which are mapped to classes)
					defaultValues[ it->name ] = it->name + "()";
					break;
				case DependencyData::Category::FUNC_POINTER: // func_pointers default to nullptr
					defaultValues[ it->name ] = "nullptr";
					break;
				case DependencyData::Category::REQUIRED:     // all required default to "0"
				case DependencyData::Category::SCALAR:       // all scalars default to "0"
					defaultValues[ it->name ] = "0";
					break;
				default:
					assert( false );
					break;
			}
		}
	}

	std::string CppGenerator::determineFunctionName( std::string const& name, CommandData const& commandData )
	{
		if( commandData.handleCommand )
		{
			std::string strippedName = name;
			std::string searchName = commandData.arguments[ 0 ].pureType;
			size_t pos = name.find( searchName );
			if( pos == std::string::npos )
			{
				assert( isupper( searchName[ 0 ] ) );
				searchName[ 0 ] = tolower( searchName[ 0 ] );
				pos = name.find( searchName );
			}
			if( pos != std::string::npos )
			{
				strippedName.erase( pos, commandData.arguments[ 0 ].pureType.length() );
			}
			else if( ( commandData.arguments[ 0 ].pureType == "CommandBuffer" ) && ( name.find( "cmd" ) == 0 ) )
			{
				strippedName.erase( 0, 3 );
				pos = 0;
			}
			if( pos == 0 )
			{
				assert( isupper( strippedName[ 0 ] ) );
				strippedName[ 0 ] = tolower( strippedName[ 0 ] );
			}
			return strippedName;
		}
		return name;
	}

	std::string CppGenerator::determineReturnType( CommandData const& commandData, size_t returnIndex, bool isVector )
	{
		std::string returnType;
		if( ( returnIndex != ~0 )
			&& ( ( commandData.returnType == "void" )
				|| ( ( commandData.returnType == "Result" )
					&& ( ( commandData.successCodes.size() == 1 )
						|| ( ( commandData.successCodes.size() == 2 )
							&& ( commandData.successCodes[ 1 ] == "eIncomplete" )
							&& commandData.twoStep ) ) ) ) )
		{
			if( isVector )
			{
				if( commandData.arguments[ returnIndex ].pureType == "void" )
				{
					returnType = "std::vector<uint8_t,Allocator>";
				}
				else
				{
					returnType = "std::vector<" + commandData.arguments[ returnIndex ].pureType + ",Allocator>";
				}
			}
			else
			{
				assert( commandData.arguments[ returnIndex ].type.back() == '*' );
				assert( commandData.arguments[ returnIndex ].type.find( "const" ) == std::string::npos );
				returnType = commandData.arguments[ returnIndex ].type;
				returnType.pop_back();
			}
		}
		else if( ( commandData.returnType == "Result" ) && ( commandData.successCodes.size() == 1 ) )
		{
			// an original return of type "Result" with less just one successCode is changed to void, errors throw an exception
			returnType = "void";
		}
		else
		{
			// the return type just stays the original return type
			returnType = commandData.returnType;
		}
		return returnType;
	}

	void CppGenerator::enterProtect( std::ofstream &ofs, std::string const& protect )
	{
		if( !protect.empty() )
		{
			ofs << "#ifdef " << protect << std::endl;
		}
	}

	std::string CppGenerator::extractTag( std::string const& name )
	{
		// the name is supposed to look like: VK_<tag>_<other>
		size_t start = name.find( '_' );
		assert( start != std::string::npos );
		size_t end = name.find( '_', start + 1 );
		assert( end != std::string::npos );
		return name.substr( start + 1, end - start - 1 );
	}

	size_t CppGenerator::findReturnIndex( CommandData const& commandData, std::map<size_t, size_t> const& vectorParameters )
	{
		if( ( commandData.returnType == "Result" ) || ( commandData.returnType == "void" ) )
		{
			for( size_t i = 0; i < commandData.arguments.size(); i++ )
			{
				if( ( commandData.arguments[ i ].type.find( '*' ) != std::string::npos ) && ( commandData.arguments[ i ].type.find( "const" ) == std::string::npos ) && !isVectorSizeParameter( vectorParameters, i )
					&& ( ( vectorParameters.find( i ) == vectorParameters.end() ) || commandData.twoStep || ( commandData.successCodes.size() == 1 ) ) )
				{
	#if !defined(NDEBUG)
					for( size_t j = i + 1; j < commandData.arguments.size(); j++ )
					{
						assert( ( commandData.arguments[ j ].type.find( '*' ) == std::string::npos ) || ( commandData.arguments[ j ].type.find( "const" ) != std::string::npos ) );
					}
	#endif
					return i;
				}
			}
		}
		return ~0;
	}

	std::string CppGenerator::findTag( std::string const& name, std::set<std::string> const& tags )
	{
		for( std::set<std::string>::const_iterator it = tags.begin(); it != tags.end(); ++it )
		{
			size_t pos = name.find( *it );
			if( ( pos != std::string::npos ) && ( pos == name.length() - it->length() ) )
			{
				return *it;
			}
		}
		return "";
	}

	size_t CppGenerator::findTemplateIndex( CommandData const& commandData, std::map<size_t, size_t> const& vectorParameters )
	{
		for( size_t i = 0; i < commandData.arguments.size(); i++ )
		{
			if( ( commandData.arguments[ i ].name == "pData" ) || ( commandData.arguments[ i ].name == "pValues" ) )
			{
				assert( vectorParameters.find( i ) != vectorParameters.end() );
				return i;
			}
		}
		return ~0;
	}

	std::string CppGenerator::getEnumName( std::string const& name ) // get vkcpp enum name from vk enum name
	{
		return strip( name, "Vk" );
	}

	std::string CppGenerator::generateEnumNameForFlags( std::string const& name )
	{
		std::string generatedName = name;
		size_t pos = generatedName.rfind( "Flags" );
		assert( pos != std::string::npos );
		generatedName.replace( pos, 5, "FlagBits" );
		return generatedName;
	}

	std::map<size_t, size_t> CppGenerator::getVectorParameters( CommandData const& commandData )
	{
		std::map<size_t, size_t> lenParameters;
		for( size_t i = 0; i < commandData.arguments.size(); i++ )
		{
			if( !commandData.arguments[ i ].len.empty() )
			{
				lenParameters.insert( std::make_pair( i, ~0 ) );
				for( size_t j = 0; j < commandData.arguments.size(); j++ )
				{
					if( commandData.arguments[ i ].len == commandData.arguments[ j ].name )
					{
						lenParameters[ i ] = j;
					}
				}
				assert( ( lenParameters[ i ] != ~0 )
					|| ( commandData.arguments[ i ].len == "dataSize/4" )
					|| ( commandData.arguments[ i ].len == "latexmath:[$dataSize \\over 4$]" )
					|| ( commandData.arguments[ i ].len == "null-terminated" )
					|| ( commandData.arguments[ i ].len == "pAllocateInfo->descriptorSetCount" )
					|| ( commandData.arguments[ i ].len == "pAllocateInfo->commandBufferCount" ) );
				assert( ( lenParameters[ i ] == ~0 ) || ( lenParameters[ i ] < i ) );
			}
		}
		return lenParameters;
	}

	bool CppGenerator::hasPointerArguments( CommandData const& commandData )
	{
		for( size_t i = 0; i < commandData.arguments.size(); i++ )
		{
			size_t pos = commandData.arguments[ i ].type.find( '*' );
			if( ( pos != std::string::npos ) && ( commandData.arguments[ i ].type.find( '*', pos + 1 ) == std::string::npos ) )
			{
				return true;
			}
		}
		return false;
	}

	bool CppGenerator::isVectorSizeParameter( std::map<size_t, size_t> const& vectorParameters, size_t idx )
	{
		for( std::map<size_t, size_t>::const_iterator it = vectorParameters.begin(); it != vectorParameters.end(); ++it )
		{
			if( it->second == idx )
			{
				return true;
			}
		}
		return false;
	}

	void CppGenerator::leaveProtect( std::ofstream &ofs, std::string const& protect )
	{
		if( !protect.empty() )
		{
			ofs << "#endif /*" << protect << "*/" << std::endl;
		}
	}

	bool CppGenerator::noDependencies( std::set<std::string> const& dependencies, std::set<std::string> & listedTypes )
	{
		bool ok = true;
		for( std::set<std::string>::const_iterator it = dependencies.begin(); it != dependencies.end() && ok; ++it )
		{
			ok = ( listedTypes.find( *it ) != listedTypes.end() );
		}
		return( ok );
	}

	bool CppGenerator::readCommandParam( tinyxml2::XMLElement * element, DependencyData & typeData, std::vector<MemberData> & arguments )
	{
		arguments.push_back( MemberData() );
		MemberData & arg = arguments.back();

		if( element->Attribute( "len" ) )
		{
			arg.len = element->Attribute( "len" );
		}

		tinyxml2::XMLNode * child = element->FirstChild();
		assert( child );
		if( child->ToText() )
		{
			std::string value = trimEnd( child->Value() );
			assert( ( value == "const" ) || ( value == "struct" ) );
			arg.type = value + " ";
			child = child->NextSibling();
			assert( child );
		}

		assert( child->ToElement() );
		assert( ( strcmp( child->Value(), "type" ) == 0 ) && child->ToElement() && child->ToElement()->GetText() );
		std::string type = strip( child->ToElement()->GetText(), "Vk" );
		typeData.dependencies.insert( type );
		arg.type += type;
		arg.pureType = type;

		child = child->NextSibling();
		assert( child );
		if( child->ToText() )
		{
			std::string value = trimEnd( child->Value() );
			assert( ( value == "*" ) || ( value == "**" ) || ( value == "* const*" ) );
			arg.type += value;
			child = child->NextSibling();
		}

		assert( child->ToElement() && ( strcmp( child->Value(), "name" ) == 0 ) );
		arg.name = child->ToElement()->GetText();

		if( arg.name.back() == ']' )
		{
			assert( !child->NextSibling() );
			size_t pos = arg.name.find( '[' );
			assert( pos != std::string::npos );
			arg.arraySize = arg.name.substr( pos + 1, arg.name.length() - 2 - pos );
			arg.name.erase( pos );
		}

		child = child->NextSibling();
		if( child )
		{
			if( child->ToText() )
			{
				std::string value = child->Value();
				if( value == "[" )
				{
					child = child->NextSibling();
					assert( child );
					assert( child->ToElement() && ( strcmp( child->Value(), "enum" ) == 0 ) );
					arg.arraySize = child->ToElement()->GetText();
					child = child->NextSibling();
					assert( child );
					assert( child->ToText() );
					assert( strcmp( child->Value(), "]" ) == 0 );
					assert( !child->NextSibling() );
				}
				else
				{
					assert( ( value.front() == '[' ) && ( value.back() == ']' ) );
					arg.arraySize = value.substr( 1, value.length() - 2 );
					assert( !child->NextSibling() );
				}
			}
		}

		arg.optional = element->Attribute( "optional" ) && ( strcmp( element->Attribute( "optional" ), "true" ) == 0 );

		return element->Attribute( "optional" ) && ( strcmp( element->Attribute( "optional" ), "false,true" ) == 0 );
	}

	std::map<std::string, CppGenerator::CommandData>::iterator CppGenerator::readCommandProto( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * typeElement = element->FirstChildElement();
		assert( typeElement && ( strcmp( typeElement->Value(), "type" ) == 0 ) );
		tinyxml2::XMLElement * nameElement = typeElement->NextSiblingElement();
		assert( nameElement && ( strcmp( nameElement->Value(), "name" ) == 0 ) );
		assert( !nameElement->NextSiblingElement() );

		std::string type = strip( typeElement->GetText(), "Vk" );
		std::string name = stripCommand( nameElement->GetText() );

		vkData.dependencies.push_back( DependencyData( DependencyData::Category::COMMAND, name ) );
		assert( vkData.commands.find( name ) == vkData.commands.end() );
		std::map<std::string, CommandData>::iterator it = vkData.commands.insert( std::make_pair( name, CommandData() ) ).first;
		it->second.returnType = type;

		return it;
	}

	void CppGenerator::readCommands( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		assert( child );
		do
		{
			assert( strcmp( child->Value(), "command" ) == 0 );
			readCommandsCommand( child, vkData );
		} while( child = child->NextSiblingElement() );
	}

	void CppGenerator::readCommandsCommand( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		assert( child && ( strcmp( child->Value(), "proto" ) == 0 ) );

		std::map<std::string, CommandData>::iterator it = readCommandProto( child, vkData );

		if( element->Attribute( "successcodes" ) )
		{
			std::string successCodes = element->Attribute( "successcodes" );
			size_t start = 0, end;
			do
			{
				end = successCodes.find( ',', start );
				std::string code = successCodes.substr( start, end - start );
				std::string tag = findTag( code, vkData.tags );
				it->second.successCodes.push_back( "e" + toCamelCase( strip( code, "VK_", tag ) ) + tag );
				start = end + 1;
			} while( end != std::string::npos );
		}

		// HACK: the current vk.xml misses to specify successcodes on command vkCreateDebugReportCallbackEXT!
		if( it->first == "createDebugReportCallbackEXT" )
		{
			it->second.successCodes.clear();
			it->second.successCodes.push_back( "eSuccess" );
		}

		while( child = child->NextSiblingElement() )
		{
			std::string value = child->Value();
			if( value == "param" )
			{
				it->second.twoStep |= readCommandParam( child, vkData.dependencies.back(), it->second.arguments );
			}
			else
			{
				assert( ( value == "implicitexternsyncparams" ) || ( value == "validity" ) );
			}
		}

		// HACK: the current vk.xml misses to specify <optional="false,true"> on param pSparseMemoryRequirementCount on command vkGetImageSparseMemoryRequirements!
		if( it->first == "getImageSparseMemoryRequirements" )
		{
			it->second.twoStep = true;
		}

		assert( !it->second.arguments.empty() );
		std::map<std::string, HandleData>::iterator hit = vkData.handles.find( it->second.arguments[ 0 ].pureType );
		if( hit != vkData.handles.end() )
		{
			hit->second.commands.push_back( it->first );
			it->second.handleCommand = true;
			DependencyData const& dep = vkData.dependencies.back();
			std::list<DependencyData>::iterator dit = std::find_if( vkData.dependencies.begin(), vkData.dependencies.end(), [ hit ]( DependencyData const& dd ) { return dd.name == hit->first; } );
			for( std::set<std::string>::const_iterator depit = dep.dependencies.begin(); depit != dep.dependencies.end(); ++depit )
			{
				if( *depit != hit->first )
				{
					dit->dependencies.insert( *depit );
				}
			}
		}
	}

	void CppGenerator::readComment( tinyxml2::XMLElement * element, std::string & header )
	{
		assert( element->GetText() );
		assert( header.empty() );
		header = element->GetText();
		assert( header.find( "\nCopyright" ) == 0 );

		size_t pos = header.find( "\n\n-----" );
		assert( pos != std::string::npos );
		header.erase( pos );

		for( size_t pos = header.find( '\n' ); pos != std::string::npos; pos = header.find( '\n', pos + 1 ) )
		{
			header.replace( pos, 1, "\n// " );
		}

		header += "\n\n// This header is generated from the Khronos Vulkan XML API Registry.";
	}

	void CppGenerator::readEnums( tinyxml2::XMLElement * element, VkData & vkData )
	{
		assert( element->Attribute( "name" ) );
		std::string name = getEnumName( element->Attribute( "name" ) );
		if( name != "API Constants" )
		{
			vkData.dependencies.push_back( DependencyData( DependencyData::Category::ENUM, name ) );
			std::map<std::string, EnumData>::iterator it = vkData.enums.insert( std::make_pair( name, EnumData() ) ).first;
			std::string tag;

			if( name == "Result" )
			{
				// special handling for VKResult, as its enums just have VK_ in common
				it->second.prefix = "VK_";
			}
			else
			{
				assert( element->Attribute( "type" ) );
				std::string type = element->Attribute( "type" );
				assert( ( type == "bitmask" ) || ( type == "enum" ) );
				it->second.bitmask = ( type == "bitmask" );
				std::string prefix, postfix;
				if( it->second.bitmask )
				{
					size_t pos = name.find( "FlagBits" );
					assert( pos != std::string::npos );
					it->second.prefix = "VK" + toUpperCase( name.substr( 0, pos ) ) + "_";
					it->second.postfix = "Bit";
				}
				else
				{
					it->second.prefix = "VK" + toUpperCase( name ) + "_";
				}

				// if the enum name contains a tag remove it from the prefix to generate correct enum value names.
				for( std::set<std::string>::const_iterator tit = vkData.tags.begin(); tit != vkData.tags.end(); ++tit )
				{
					size_t pos = it->second.prefix.find( *tit );
					if( ( pos != std::string::npos ) && ( pos == it->second.prefix.length() - tit->length() - 1 ) )
					{
						it->second.prefix.erase( pos );
						tag = *tit;
						break;
					}
				}
			}

			readEnumsEnum( element, it->second, tag );

			assert( vkData.vkTypes.find( name ) == vkData.vkTypes.end() );
			vkData.vkTypes.insert( name );
		}
	}

	void CppGenerator::readEnumsEnum( tinyxml2::XMLElement * element, EnumData & enumData, std::string const& tag )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		do
		{
			if( child->Attribute( "name" ) )
			{
				enumData.addEnum( child->Attribute( "name" ), tag, false );
			}
		} while( child = child->NextSiblingElement() );
	}

	void CppGenerator::readExtensionRequire( tinyxml2::XMLElement * element, VkData & vkData, std::string const& protect, std::string const& tag )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		do
		{
			std::string value = child->Value();

			if( value == "command" )
			{
				assert( child->Attribute( "name" ) );
				std::string name = stripCommand( child->Attribute( "name" ) );
				std::map<std::string, CommandData>::iterator cit = vkData.commands.find( name );
				assert( cit != vkData.commands.end() );
				cit->second.protect = protect;
			}
			else if( value == "type" )
			{
				assert( child->Attribute( "name" ) );
				std::string name = strip( child->Attribute( "name" ), "Vk" );
				std::map<std::string, EnumData>::iterator eit = vkData.enums.find( name );
				if( eit != vkData.enums.end() )
				{
					eit->second.protect = protect;
				}
				else
				{
					std::map<std::string, FlagData>::iterator fit = vkData.flags.find( name );
					if( fit != vkData.flags.end() )
					{
						fit->second.protect = protect;

						// if the enum of this flags is auto-generated, protect it as well
						std::string enumName = generateEnumNameForFlags( name );
						std::map<std::string, EnumData>::iterator eit = vkData.enums.find( enumName );
						assert( eit != vkData.enums.end() );
						if( eit->second.members.empty() )
						{
							eit->second.protect = protect;
						}
					}
					else
					{
						std::map<std::string, ScalarData>::iterator scit = vkData.scalars.find( name );
						if( scit != vkData.scalars.end() )
						{
							scit->second.protect = protect;
						}
						else
						{
							std::map<std::string, StructData>::iterator stit = vkData.structs.find( name );
							assert( stit != vkData.structs.end() && stit->second.protect.empty() );
							stit->second.protect = protect;
						}
					}
				}
			}
			else if( value == "enum" )
			{
				// TODO process enums which don't extend existing enums
				if( child->Attribute( "extends" ) )
				{
					assert( child->Attribute( "name" ) );
					assert( vkData.enums.find( getEnumName( child->Attribute( "extends" ) ) ) != vkData.enums.end() );
					assert( !!child->Attribute( "bitpos" ) + !!child->Attribute( "offset" ) + !!child->Attribute( "value" ) == 1 );
					vkData.enums[ getEnumName( child->Attribute( "extends" ) ) ].addEnum( child->Attribute( "name" ), child->Attribute( "value" ) ? "" : tag, true );
				}
			}
			else
			{
				assert( value == "usage" );
			}
		} while( child = child->NextSiblingElement() );
	}

	void CppGenerator::readExtensions( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		assert( child );
		do
		{
			assert( strcmp( child->Value(), "extension" ) == 0 );
			readExtensionsExtension( child, vkData );
		} while( child = child->NextSiblingElement() );
	}

	void CppGenerator::readExtensionsExtension( tinyxml2::XMLElement * element, VkData & vkData )
	{
		assert( element->Attribute( "name" ) );
		std::string tag = extractTag( element->Attribute( "name" ) );
		assert( vkData.tags.find( tag ) != vkData.tags.end() );

		// don't parse disabled extensions
		if( strcmp( element->Attribute( "supported" ), "disabled" ) == 0 )
		{
			return;
		}

		std::string protect;
		if( element->Attribute( "protect" ) )
		{
			protect = element->Attribute( "protect" );
		}

		tinyxml2::XMLElement * child = element->FirstChildElement();
		assert( child && ( strcmp( child->Value(), "require" ) == 0 ) && !child->NextSiblingElement() );
		readExtensionRequire( child, vkData, protect, tag );
	}

	void CppGenerator::readTypeBasetype( tinyxml2::XMLElement * element, std::list<DependencyData> & dependencies )
	{
		tinyxml2::XMLElement * typeElement = element->FirstChildElement();
		assert( typeElement && ( strcmp( typeElement->Value(), "type" ) == 0 ) && typeElement->GetText() );
		std::string type = typeElement->GetText();
		assert( ( type == "uint32_t" ) || ( type == "uint64_t" ) );

		tinyxml2::XMLElement * nameElement = typeElement->NextSiblingElement();
		assert( nameElement && ( strcmp( nameElement->Value(), "name" ) == 0 ) && nameElement->GetText() );
		std::string name = strip( nameElement->GetText(), "Vk" );

		// skip "Flags",
		if( name != "Flags" )
		{
			dependencies.push_back( DependencyData( DependencyData::Category::SCALAR, name ) );
			dependencies.back().dependencies.insert( type );
		}
		else
		{
			assert( type == "uint32_t" );
		}
	}

	void CppGenerator::readTypeBitmask( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * typeElement = element->FirstChildElement();
		assert( typeElement && ( strcmp( typeElement->Value(), "type" ) == 0 ) && typeElement->GetText() && ( strcmp( typeElement->GetText(), "VkFlags" ) == 0 ) );
		std::string type = typeElement->GetText();

		tinyxml2::XMLElement * nameElement = typeElement->NextSiblingElement();
		assert( nameElement && ( strcmp( nameElement->Value(), "name" ) == 0 ) && nameElement->GetText() );
		std::string name = strip( nameElement->GetText(), "Vk" );

		assert( !nameElement->NextSiblingElement() );

		std::string requires;
		if( element->Attribute( "requires" ) )
		{
			requires = strip( element->Attribute( "requires" ), "Vk" );
		}
		else
		{
			// Generate FlagBits name
			requires = generateEnumNameForFlags( name );
			vkData.dependencies.push_back( DependencyData( DependencyData::Category::ENUM, requires ) );
			std::map<std::string, EnumData>::iterator it = vkData.enums.insert( std::make_pair( requires, EnumData() ) ).first;
			it->second.bitmask = true;
			vkData.vkTypes.insert( requires );
		}

		vkData.dependencies.push_back( DependencyData( DependencyData::Category::FLAGS, name ) );
		vkData.dependencies.back().dependencies.insert( requires );
		vkData.flags.insert( std::make_pair( name, FlagData() ) );

		assert( vkData.vkTypes.find( name ) == vkData.vkTypes.end() );
		vkData.vkTypes.insert( name );
	}

	void CppGenerator::readTypeDefine( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		if( child && ( strcmp( child->GetText(), "VK_HEADER_VERSION" ) == 0 ) )
		{
			vkData.version = element->LastChild()->ToText()->Value();
		}
		else if( element->Attribute( "name" ) && strcmp( element->Attribute( "name" ), "VK_DEFINE_NON_DISPATCHABLE_HANDLE" ) == 0 )
		{
			std::string text = element->LastChild()->ToText()->Value();
			size_t start = text.find( '#' );
			size_t end = text.find_first_of( "\r\n", start + 1 );
			vkData.typesafeCheck = text.substr( start, end - start );
		}
	}

	void CppGenerator::readTypeFuncpointer( tinyxml2::XMLElement * element, std::list<DependencyData> & dependencies )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		assert( child && ( strcmp( child->Value(), "name" ) == 0 ) && child->GetText() );
		dependencies.push_back( DependencyData( DependencyData::Category::FUNC_POINTER, child->GetText() ) );
	}

	void CppGenerator::readTypeHandle( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * typeElement = element->FirstChildElement();
		assert( typeElement && ( strcmp( typeElement->Value(), "type" ) == 0 ) && typeElement->GetText() );
	#if !defined(NDEBUG)
		std::string type = typeElement->GetText();
		assert( type.find( "VK_DEFINE" ) == 0 );
	#endif

		tinyxml2::XMLElement * nameElement = typeElement->NextSiblingElement();
		assert( nameElement && ( strcmp( nameElement->Value(), "name" ) == 0 ) && nameElement->GetText() );
		std::string name = strip( nameElement->GetText(), "Vk" );

		vkData.dependencies.push_back( DependencyData( DependencyData::Category::HANDLE, name ) );

		assert( vkData.vkTypes.find( name ) == vkData.vkTypes.end() );
		vkData.vkTypes.insert( name );
		assert( vkData.handles.find( name ) == vkData.handles.end() );
		vkData.handles[ name ];    // add this to the handles map
	}

	void CppGenerator::readTypeStructMember( tinyxml2::XMLElement * element, std::vector<MemberData> & members, std::set<std::string> & dependencies )
	{
		members.push_back( MemberData() );
		MemberData & member = members.back();

		tinyxml2::XMLNode * child = element->FirstChild();
		assert( child );
		if( child->ToText() )
		{
			std::string value = trimEnd( child->Value() );
			assert( ( value == "const" ) || ( value == "struct" ) );
			member.type = value + " ";
			child = child->NextSibling();
			assert( child );
		}

		assert( child->ToElement() );
		assert( ( strcmp( child->Value(), "type" ) == 0 ) && child->ToElement() && child->ToElement()->GetText() );
		std::string type = strip( child->ToElement()->GetText(), "Vk" );
		dependencies.insert( type );
		member.type += type;
		member.pureType = type;

		child = child->NextSibling();
		assert( child );
		if( child->ToText() )
		{
			std::string value = trimEnd( child->Value() );
			assert( ( value == "*" ) || ( value == "**" ) || ( value == "* const*" ) );
			member.type += value;
			child = child->NextSibling();
		}

		assert( ( child->ToElement() && strcmp( child->Value(), "name" ) == 0 ) );
		member.name = child->ToElement()->GetText();

		if( member.name.back() == ']' )
		{
			assert( !child->NextSibling() );
			size_t pos = member.name.find( '[' );
			assert( pos != std::string::npos );
			member.arraySize = member.name.substr( pos + 1, member.name.length() - 2 - pos );
			member.name.erase( pos );
		}

		child = child->NextSibling();
		if( child )
		{
			assert( member.arraySize.empty() );
			if( child->ToText() )
			{
				std::string value = child->Value();
				if( value == "[" )
				{
					child = child->NextSibling();
					assert( child );
					assert( child->ToElement() && ( strcmp( child->Value(), "enum" ) == 0 ) );
					member.arraySize = child->ToElement()->GetText();
					child = child->NextSibling();
					assert( child );
					assert( child->ToText() );
					assert( strcmp( child->Value(), "]" ) == 0 );
					assert( !child->NextSibling() );
				}
				else
				{
					assert( ( value.front() == '[' ) && ( value.back() == ']' ) );
					member.arraySize = value.substr( 1, value.length() - 2 );
					assert( !child->NextSibling() );
				}
			}
		}
	}

	void CppGenerator::readTypeStruct( tinyxml2::XMLElement * element, VkData & vkData )
	{
		assert( !element->Attribute( "returnedonly" ) || ( strcmp( element->Attribute( "returnedonly" ), "true" ) == 0 ) );

		assert( element->Attribute( "name" ) );
		std::string name = strip( element->Attribute( "name" ), "Vk" );

		if( name == "Rect3D" )
		{
			return;
		}

		vkData.dependencies.push_back( DependencyData( DependencyData::Category::STRUCT, name ) );

		assert( vkData.structs.find( name ) == vkData.structs.end() );
		std::map<std::string, StructData>::iterator it = vkData.structs.insert( std::make_pair( name, StructData() ) ).first;
		it->second.returnedOnly = !!element->Attribute( "returnedonly" );

		tinyxml2::XMLElement * child = element->FirstChildElement();
		do
		{
			assert( child->Value() );
			std::string value = child->Value();
			if( value == "member" )
			{
				readTypeStructMember( child, it->second.members, vkData.dependencies.back().dependencies );
			}
			else
			{
				assert( value == "validity" );
			}
		} while( child = child->NextSiblingElement() );

		assert( vkData.vkTypes.find( name ) == vkData.vkTypes.end() );
		vkData.vkTypes.insert( name );
	}

	void CppGenerator::readTypeUnionMember( tinyxml2::XMLElement * element, std::vector<MemberData> & members, std::set<std::string> & dependencies )
	{
		members.push_back( MemberData() );
		MemberData & member = members.back();

		tinyxml2::XMLNode * child = element->FirstChild();
		assert( child );
		if( child->ToText() )
		{
			assert( ( strcmp( child->Value(), "const" ) == 0 ) || ( strcmp( child->Value(), "struct" ) == 0 ) );
			member.type = std::string( child->Value() ) + " ";
			child = child->NextSibling();
			assert( child );
		}

		assert( child->ToElement() );
		assert( ( strcmp( child->Value(), "type" ) == 0 ) && child->ToElement() && child->ToElement()->GetText() );
		std::string type = strip( child->ToElement()->GetText(), "Vk" );
		dependencies.insert( type );
		member.type += type;
		member.pureType = type;

		child = child->NextSibling();
		assert( child );
		if( child->ToText() )
		{
			std::string value = child->Value();
			assert( ( value == "*" ) || ( value == "**" ) || ( value == "* const*" ) );
			member.type += value;
			child = child->NextSibling();
		}

		assert( child->ToElement() && ( strcmp( child->Value(), "name" ) == 0 ) );
		member.name = child->ToElement()->GetText();

		if( member.name.back() == ']' )
		{
			assert( !child->NextSibling() );
			size_t pos = member.name.find( '[' );
			assert( pos != std::string::npos );
			member.arraySize = member.name.substr( pos + 1, member.name.length() - 2 - pos );
			member.name.erase( pos );
		}

		child = child->NextSibling();
		if( child )
		{
			if( child->ToText() )
			{
				std::string value = child->Value();
				if( value == "[" )
				{
					child = child->NextSibling();
					assert( child );
					assert( child->ToElement() && ( strcmp( child->Value(), "enum" ) == 0 ) );
					member.arraySize = child->ToElement()->GetText();
					child = child->NextSibling();
					assert( child );
					assert( child->ToText() );
					assert( strcmp( child->Value(), "]" ) == 0 );
					assert( !child->NextSibling() );
				}
				else
				{
					assert( ( value.front() == '[' ) && ( value.back() == ']' ) );
					member.arraySize = value.substr( 1, value.length() - 2 );
					assert( !child->NextSibling() );
				}
			}
		}
	}

	void CppGenerator::readTypeUnion( tinyxml2::XMLElement * element, VkData & vkData )
	{
		assert( element->Attribute( "name" ) );
		std::string name = strip( element->Attribute( "name" ), "Vk" );

		vkData.dependencies.push_back( DependencyData( DependencyData::Category::UNION, name ) );

		assert( vkData.structs.find( name ) == vkData.structs.end() );
		std::map<std::string, StructData>::iterator it = vkData.structs.insert( std::make_pair( name, StructData() ) ).first;

		tinyxml2::XMLElement * child = element->FirstChildElement();
		do
		{
			assert( strcmp( child->Value(), "member" ) == 0 );
			readTypeUnionMember( child, it->second.members, vkData.dependencies.back().dependencies );
		} while( child = child->NextSiblingElement() );

		assert( vkData.vkTypes.find( name ) == vkData.vkTypes.end() );
		vkData.vkTypes.insert( name );
	}

	void CppGenerator::readTags( tinyxml2::XMLElement * element, std::set<std::string> & tags )
	{
		tags.insert( "EXT" );
		tags.insert( "KHR" );
		tinyxml2::XMLElement * child = element->FirstChildElement();
		do
		{
			assert( child->Attribute( "name" ) );
			tags.insert( child->Attribute( "name" ) );
		} while( child = child->NextSiblingElement() );
	}

	void CppGenerator::readTypes( tinyxml2::XMLElement * element, VkData & vkData )
	{
		tinyxml2::XMLElement * child = element->FirstChildElement();
		do
		{
			assert( strcmp( child->Value(), "type" ) == 0 );
			std::string type = child->Value();
			assert( type == "type" );
			if( child->Attribute( "category" ) )
			{
				std::string category = child->Attribute( "category" );
				if( category == "basetype" )
				{
					readTypeBasetype( child, vkData.dependencies );
				}
				else if( category == "bitmask" )
				{
					readTypeBitmask( child, vkData );
				}
				else if( category == "define" )
				{
					readTypeDefine( child, vkData );
				}
				else if( category == "funcpointer" )
				{
					readTypeFuncpointer( child, vkData.dependencies );
				}
				else if( category == "handle" )
				{
					readTypeHandle( child, vkData );
				}
				else if( category == "struct" )
				{
					readTypeStruct( child, vkData );
				}
				else if( category == "union" )
				{
					readTypeUnion( child, vkData );
				}
				else
				{
					assert( ( category == "enum" ) || ( category == "include" ) );
				}
			}
			else
			{
				assert( child->Attribute( "requires" ) && child->Attribute( "name" ) );
				vkData.dependencies.push_back( DependencyData( DependencyData::Category::REQUIRED, child->Attribute( "name" ) ) );
			}
		} while( child = child->NextSiblingElement() );
	}

	void CppGenerator::sortDependencies( std::list<DependencyData> & dependencies )
	{
		std::set<std::string> listedTypes = { "VkFlags" };
		std::list<DependencyData> sortedDependencies;

		while( !dependencies.empty() )
		{
	#if !defined(NDEBUG)
			bool ok = false;
	#endif
			for( std::list<DependencyData>::iterator it = dependencies.begin(); it != dependencies.end(); ++it )
			{
				if( noDependencies( it->dependencies, listedTypes ) )
				{
					sortedDependencies.push_back( *it );
					listedTypes.insert( it->name );
					dependencies.erase( it );
	#if !defined(NDEBUG)
					ok = true;
	#endif
					break;
				}
			}
			assert( ok );
		}

		dependencies.swap( sortedDependencies );
	}

	std::string CppGenerator::reduceName( std::string const& name )
	{
		std::string reducedName;
		if( ( name[ 0 ] == 'p' ) && ( 1 < name.length() ) && ( isupper( name[ 1 ] ) || name[ 1 ] == 'p' ) )
		{
			reducedName = strip( name, "p" );
			reducedName[ 0 ] = tolower( reducedName[ 0 ] );
		}
		else
		{
			reducedName = name;
		}

		return reducedName;
	}

	std::string CppGenerator::strip( std::string const& value, std::string const& prefix, std::string const& postfix )
	{
		std::string strippedValue = value;
		if( strippedValue.find( prefix ) == 0 )
		{
			strippedValue.erase( 0, prefix.length() );
		}
		if( !postfix.empty() )
		{
			size_t pos = strippedValue.rfind( postfix );
			assert( pos != std::string::npos );
			strippedValue.erase( pos );
		}
		return strippedValue;
	}

	std::string CppGenerator::stripCommand( std::string const& value )
	{
		std::string stripped = strip( value, "vk" );
		assert( isupper( stripped[ 0 ] ) );
		stripped[ 0 ] = tolower( stripped[ 0 ] );
		return stripped;
	}

	std::string CppGenerator::toCamelCase( std::string const& value )
	{
		assert( !value.empty() && ( isupper( value[ 0 ] ) || isdigit( value[ 0 ] ) ) );
		std::string result;
		result.reserve( value.size() );
		result.push_back( value[ 0 ] );
		for( size_t i = 1; i < value.size(); i++ )
		{
			if( value[ i ] != '_' )
			{
				if( ( value[ i - 1 ] == '_' ) || isdigit( value[ i - 1 ] ) )
				{
					result.push_back( value[ i ] );
				}
				else
				{
					result.push_back( tolower( value[ i ] ) );
				}
			}
		}
		return result;
	}

	std::string CppGenerator::toUpperCase( std::string const& name )
	{
		assert( isupper( name.front() ) );
		std::string convertedName;

		for( size_t i = 0; i < name.length(); i++ )
		{
			if( isupper( name[ i ] ) && ( ( i == 0 ) || islower( name[ i - 1 ] ) || isdigit( name[ i - 1 ] ) ) )
			{
				convertedName.push_back( '_' );
			}
			convertedName.push_back( toupper( name[ i ] ) );
		}
		return convertedName;
	}

	void CppGenerator::writeCall( std::ofstream & ofs, std::string const& name, size_t templateIndex, CommandData const& commandData, std::set<std::string> const& vkTypes, std::map<size_t, size_t> const& vectorParameters, size_t returnIndex, bool firstCall )
	{
		std::map<size_t, size_t> countIndices;
		for( std::map<size_t, size_t>::const_iterator it = vectorParameters.begin(); it != vectorParameters.end(); ++it )
		{
			countIndices.insert( std::make_pair( it->second, it->first ) );
		}
		if( ( vectorParameters.size() == 1 )
			&& ( ( commandData.arguments[ vectorParameters.begin()->first ].len == "dataSize/4" ) || ( commandData.arguments[ vectorParameters.begin()->first ].len == "latexmath:[$dataSize \\over 4$]" ) ) )
		{
			assert( commandData.arguments[ 3 ].name == "dataSize" );
			countIndices.insert( std::make_pair( 3, vectorParameters.begin()->first ) );
		}

		assert( islower( name[ 0 ] ) );
		ofs << "vk" << static_cast<char>( toupper( name[ 0 ] ) ) << name.substr( 1 ) << "( ";
		size_t i = 0;
		if( commandData.handleCommand )
		{
			ofs << "m_" << commandData.arguments[ 0 ].name;
			i++;
		}
		for( ; i < commandData.arguments.size(); i++ )
		{
			if( 0 < i )
			{
				ofs << ", ";
			}
			std::map<size_t, size_t>::const_iterator it = countIndices.find( i );
			if( it != countIndices.end() )
			{
				if( ( returnIndex == it->second ) && commandData.twoStep )
				{
					ofs << "&" << reduceName( commandData.arguments[ it->first ].name );
				}
				else
				{
					ofs << reduceName( commandData.arguments[ it->second ].name ) << ".size() ";
					if( templateIndex == it->second )
					{
						ofs << "* sizeof( T ) ";
					}
				}
			}
			else
			{
				it = vectorParameters.find( i );
				if( it != vectorParameters.end() )
				{
					assert( commandData.arguments[ it->first ].type.back() == '*' );
					if( ( returnIndex == it->first ) && commandData.twoStep && firstCall )
					{
						ofs << "nullptr";
					}
					else
					{
						std::set<std::string>::const_iterator vkit = vkTypes.find( commandData.arguments[ it->first ].pureType );
						if( ( vkit != vkTypes.end() ) || ( it->first == templateIndex ) )
						{
							ofs << "reinterpret_cast<";
							if( commandData.arguments[ it->first ].type.find( "const" ) != std::string::npos )
							{
								ofs << "const ";
							}
							if( vkit != vkTypes.end() )
							{
								ofs << "Vk";
							}
							ofs << commandData.arguments[ it->first ].pureType << "*>( " << reduceName( commandData.arguments[ it->first ].name ) << ".data() )";
						}
						else if( commandData.arguments[ it->first ].pureType == "char" )
						{
							ofs << reduceName( commandData.arguments[ it->first ].name );
							if( commandData.arguments[ it->first ].optional )
							{
								ofs << " ? " << reduceName( commandData.arguments[ it->first ].name ) << "->c_str() : nullptr";
							}
							else
							{
								ofs << ".c_str()";
							}
						}
						else
						{
							ofs << reduceName( commandData.arguments[ it->first ].name ) << ".data()";
						}
					}
				}
				else if( vkTypes.find( commandData.arguments[ i ].pureType ) != vkTypes.end() )
				{
					if( commandData.arguments[ i ].type.back() == '*' )
					{
						if( commandData.arguments[ i ].type.find( "const" ) != std::string::npos )
						{
							ofs << "reinterpret_cast<const Vk" << commandData.arguments[ i ].pureType << "*>( ";
							if( commandData.arguments[ i ].optional )
							{
								ofs << "static_cast<const " << commandData.arguments[ i ].pureType << "*>( ";
							}
							else
							{
								ofs << "&";
							}
							ofs << reduceName( commandData.arguments[ i ].name ) << ( commandData.arguments[ i ].optional ? "))" : " )" );
						}
						else
						{
							assert( !commandData.arguments[ i ].optional );
							ofs << "reinterpret_cast<Vk" << commandData.arguments[ i ].pureType << "*>( &" << reduceName( commandData.arguments[ i ].name ) << " )";
						}
					}
					else
					{
						ofs << "static_cast<Vk" << commandData.arguments[ i ].pureType << ">( " << commandData.arguments[ i ].name << " )";
					}
				}
				else
				{
					if( commandData.arguments[ i ].type.back() == '*' )
					{
						if( commandData.arguments[ i ].type.find( "const" ) != std::string::npos )
						{
							assert( commandData.arguments[ i ].type.find( "char" ) != std::string::npos );
							ofs << reduceName( commandData.arguments[ i ].name );
							if( commandData.arguments[ i ].optional )
							{
								ofs << " ? " << reduceName( commandData.arguments[ i ].name ) << "->c_str() : nullptr";
							}
							else
							{
								ofs << ".c_str()";
							}
						}
						else
						{
							assert( commandData.arguments[ i ].type.find( "char" ) == std::string::npos );
							ofs << "&" << reduceName( commandData.arguments[ i ].name );
						}
					}
					else
					{
						ofs << commandData.arguments[ i ].name;
					}
				}
			}
		}
		ofs << " )";
	}

	void CppGenerator::writeExceptionCheck( std::ofstream & ofs, std::string const& indentation, std::string const& className, std::string const& functionName, std::vector<std::string> const& successCodes )
	{
		assert( !successCodes.empty() );
		ofs << indentation << "  if (";
		if( successCodes.size() == 1 )
		{
			assert( successCodes.front() == "eSuccess" );
			ofs << " result != Result::eSuccess";
		}
		else
		{
			for( size_t i = 0; i < successCodes.size() - 1; i++ )
			{
				ofs << " ( result != Result::" << successCodes[ i ] << " ) &&";
			}
			ofs << " ( result != Result::" << successCodes.back() << " )";
		}
		ofs << " )" << std::endl;
		ofs << indentation << "  {" << std::endl
			<< indentation << "    throw std::system_error( result, \"vk::";
		if( !className.empty() )
		{
			ofs << className << "::";
		}
		ofs << functionName << "\" );" << std::endl
			<< indentation << "  }" << std::endl;
	}

	void CppGenerator::writeFunctionBody( std::ofstream & ofs, std::string const& indentation, std::string const& className, std::string const& functionName, std::string const& returnType, size_t templateIndex, DependencyData const& dependencyData, CommandData const& commandData, std::set<std::string> const& vkTypes, size_t returnIndex, std::map<size_t, size_t> const& vectorParameters )
	{
		ofs << indentation << "{" << std::endl;

		// add a static_assert if a type is templated and its size needs to be some multiple of the original size
		if( ( templateIndex != ~0 ) && ( commandData.arguments[ templateIndex ].pureType != "void" ) )
		{
			ofs << indentation << "  static_assert( sizeof( T ) % sizeof( " << commandData.arguments[ templateIndex ].pureType << " ) == 0, \"wrong size of template type T\" );" << std::endl;
		}

		// add some error checks if multiple vectors need to have the same size
		if( 1 < vectorParameters.size() )
		{
			for( std::map<size_t, size_t>::const_iterator it0 = vectorParameters.begin(); it0 != vectorParameters.end(); ++it0 )
			{
				if( it0->first != returnIndex )
				{
					for( std::map<size_t, size_t>::const_iterator it1 = std::next( it0 ); it1 != vectorParameters.end(); ++it1 )
					{
						if( ( it1->first != returnIndex ) && ( it0->second == it1->second ) )
						{
							ofs << "#ifdef VK_CPP_NO_EXCEPTIONS" << std::endl
								<< indentation << "  assert( " << reduceName( commandData.arguments[ it0->first ].name ) << ".size() == " << reduceName( commandData.arguments[ it1->first ].name ) << ".size() );" << std::endl
								<< "#else" << std::endl
								<< indentation << "  if ( " << reduceName( commandData.arguments[ it0->first ].name ) << ".size() != " << reduceName( commandData.arguments[ it1->first ].name ) << ".size() )" << std::endl
								<< indentation << "  {" << std::endl
								<< indentation << "    throw std::logic_error( \"vk::" << className << "::" << functionName << ": " << reduceName( commandData.arguments[ it0->first ].name ) << ".size() != " << reduceName( commandData.arguments[ it1->first ].name ) << ".size()\" );" << std::endl
								<< indentation << "  }" << std::endl
								<< "#endif  // VK_CPP_NO_EXCEPTIONS" << std::endl;
						}
					}
				}
			}
		}

		// write the local variable to hold a returned value
		if( returnIndex != ~0 )
		{
			if( commandData.returnType != returnType )
			{
				ofs << indentation << "  " << returnType << " " << reduceName( commandData.arguments[ returnIndex ].name );

				std::map<size_t, size_t>::const_iterator it = vectorParameters.find( returnIndex );
				if( it != vectorParameters.end() && !commandData.twoStep )
				{
					std::string size;
					if( ( it->second == ~0 ) && !commandData.arguments[ returnIndex ].len.empty() )
					{
						size = reduceName( commandData.arguments[ returnIndex ].len );
						size_t pos = size.find( "->" );
						assert( pos != std::string::npos );
						size.replace( pos, 2, "." );
					}
					else
					{
						for( std::map<size_t, size_t>::const_iterator sit = vectorParameters.begin(); sit != vectorParameters.end(); ++sit )
						{
							if( ( sit->first != returnIndex ) && ( sit->second == it->second ) )
							{
								size = reduceName( commandData.arguments[ sit->first ].name ) + ".size()";
								break;
							}
						}
					}
					assert( !size.empty() );
					ofs << "( " << size << " )";
				}
				ofs << ";" << std::endl;
			}
			else if( 1 < commandData.successCodes.size() )
			{
				ofs << indentation << "  " << commandData.arguments[ returnIndex ].pureType << " " << reduceName( commandData.arguments[ returnIndex ].name ) << ";" << std::endl;
			}
		}

		// local count variable to hold the size of the vector to fill
		if( commandData.twoStep )
		{
			assert( returnIndex != ~0 );

			std::map<size_t, size_t>::const_iterator returnit = vectorParameters.find( returnIndex );
			assert( returnit != vectorParameters.end() && ( returnit->second != ~0 ) );
			assert( ( commandData.returnType == "Result" ) || ( commandData.returnType == "void" ) );

			ofs << indentation << "  " << commandData.arguments[ returnit->second ].pureType << " " << reduceName( commandData.arguments[ returnit->second ].name ) << ";" << std::endl;
		}

		// write the function call
		ofs << indentation << "  ";
		std::string localIndentation = "  ";
		if( commandData.returnType == "Result" )
		{
			ofs << "Result result";
			if( commandData.twoStep && ( 1 < commandData.successCodes.size() ) )
			{
				ofs << ";" << std::endl
					<< indentation << "  do" << std::endl
					<< indentation << "  {" << std::endl
					<< indentation << "    result";
				localIndentation += "  ";
			}
			ofs << " = static_cast<Result>( ";
		}
		else if( commandData.returnType != "void" )
		{
			assert( !commandData.twoStep );
			ofs << "return ";
		}
		writeCall( ofs, dependencyData.name, templateIndex, commandData, vkTypes, vectorParameters, returnIndex, true );
		if( commandData.returnType == "Result" )
		{
			ofs << " )";
		}
		ofs << ";" << std::endl;

		if( commandData.twoStep )
		{
			std::map<size_t, size_t>::const_iterator returnit = vectorParameters.find( returnIndex );

			if( commandData.returnType == "Result" )
			{
				ofs << indentation << localIndentation << "if ( ( result == Result::eSuccess ) && " << reduceName( commandData.arguments[ returnit->second ].name ) << " )" << std::endl
					<< indentation << localIndentation << "{" << std::endl
					<< indentation << localIndentation << "  ";
			}
			else
			{
				ofs << indentation << "  ";
			}

			// resize the vector to hold the data according to the result from the first call
			ofs << reduceName( commandData.arguments[ returnit->first ].name ) << ".resize( " << reduceName( commandData.arguments[ returnit->second ].name ) << " );" << std::endl;

			// write the function call a second time
			if( commandData.returnType == "Result" )
			{
				ofs << indentation << localIndentation << "  result = static_cast<Result>( ";
			}
			else
			{
				ofs << indentation << "  ";
			}
			writeCall( ofs, dependencyData.name, templateIndex, commandData, vkTypes, vectorParameters, returnIndex, false );
			if( commandData.returnType == "Result" )
			{
				ofs << " )";
			}
			ofs << ";" << std::endl;
			if( commandData.returnType == "Result" )
			{
				ofs << indentation << localIndentation << "}" << std::endl;
				if( 1 < commandData.successCodes.size() )
				{
					ofs << indentation << "  } while ( result == Result::eIncomplete );" << std::endl;
				}
			}
		}

		if( ( commandData.returnType == "Result" ) || !commandData.successCodes.empty() )
		{
			ofs << indentation << "  return createResultValue( result, ";
			if( returnIndex != ~0 )
			{
				ofs << reduceName( commandData.arguments[ returnIndex ].name ) << ", ";
			}
			ofs << "\"vk::" << ( className.empty() ? "" : className + "::" ) << functionName << "\"";
			if( 1 < commandData.successCodes.size() && !commandData.twoStep )
			{
				ofs << ", { Result::" << commandData.successCodes[ 0 ];
				for( size_t i = 1; i < commandData.successCodes.size(); i++ )
				{
					ofs << ", Result::" << commandData.successCodes[ i ];
				}
				ofs << " }";
			}
			ofs << " );" << std::endl;
		}
		else if( ( returnIndex != ~0 ) && ( commandData.returnType != returnType ) )
		{
			ofs << indentation << "  return " << reduceName( commandData.arguments[ returnIndex ].name ) << ";" << std::endl;
		}

		ofs << indentation << "}" << std::endl;
	}

	void CppGenerator::writeFunctionHeader( std::ofstream & ofs, VkData const& vkData, std::string const& indentation, std::string const& returnType, std::string const& name, CommandData const& commandData, size_t returnIndex, size_t templateIndex, std::map<size_t, size_t> const& vectorParameters )
	{
		std::set<size_t> skippedArguments;
		for( std::map<size_t, size_t>::const_iterator it = vectorParameters.begin(); it != vectorParameters.end(); ++it )
		{
			if( it->second != ~0 )
			{
				skippedArguments.insert( it->second );
			}
		}
		if( ( vectorParameters.size() == 1 )
			&& ( ( commandData.arguments[ vectorParameters.begin()->first ].len == "dataSize/4" ) || ( commandData.arguments[ vectorParameters.begin()->first ].len == "latexmath:[$dataSize \\over 4$]" ) ) )
		{
			assert( commandData.arguments[ 3 ].name == "dataSize" );
			skippedArguments.insert( 3 );
		}
		if( returnIndex != ~0 )
		{
			skippedArguments.insert( returnIndex );
		}

		ofs << indentation;
		if( ( templateIndex != ~0 ) && ( ( templateIndex != returnIndex ) || ( returnType == "Result" ) ) )
		{
			assert( returnType.find( "Allocator" ) == std::string::npos );
			ofs << "template <typename T>" << std::endl
				<< indentation;
		}
		else if( returnType.find( "Allocator" ) != std::string::npos )
		{
			assert( ( returnType.substr( 0, 12 ) == "std::vector<" ) && ( returnType.find( ',' ) != std::string::npos ) && ( 12 < returnType.find( ',' ) ) );
			ofs << "template <typename Allocator = std::allocator<" << returnType.substr( 12, returnType.find( ',' ) - 12 ) << ">>" << std::endl
				<< indentation;
			if( ( returnType != commandData.returnType ) && ( commandData.returnType != "void" ) )
			{
				ofs << "typename ";
			}
		}
		else if( !commandData.handleCommand )
		{
			ofs << "inline ";
		}
		if( ( returnType != commandData.returnType ) && ( commandData.returnType != "void" ) )
		{
			assert( commandData.returnType == "Result" );
			ofs << "ResultValueType<" << returnType << ">::type ";
		}
		else if( ( returnIndex != ~0 ) && ( 1 < commandData.successCodes.size() ) )
		{
			assert( commandData.returnType == "Result" );
			ofs << "ResultValue<" << commandData.arguments[ returnIndex ].pureType << "> ";
		}
		else
		{
			ofs << returnType << " ";
		}
		ofs << reduceName( name ) << "(";
		if( skippedArguments.size() + ( commandData.handleCommand ? 1 : 0 ) < commandData.arguments.size() )
		{
			size_t lastArgument = ~0;
			for( size_t i = commandData.arguments.size() - 1; i < commandData.arguments.size(); i-- )
			{
				if( skippedArguments.find( i ) == skippedArguments.end() )
				{
					lastArgument = i;
					break;
				}
			}

			ofs << " ";
			bool argEncountered = false;
			for( size_t i = commandData.handleCommand ? 1 : 0; i < commandData.arguments.size(); i++ )
			{
				if( skippedArguments.find( i ) == skippedArguments.end() )
				{
					if( argEncountered )
					{
						ofs << ", ";
					}

					std::map<size_t, size_t>::const_iterator it = vectorParameters.find( i );
					size_t pos = commandData.arguments[ i ].type.find( '*' );
					if( it == vectorParameters.end() )
					{
						if( pos == std::string::npos )
						{
							ofs << commandData.arguments[ i ].type << " " << reduceName( commandData.arguments[ i ].name );
							if( !commandData.arguments[ i ].arraySize.empty() )
							{
								ofs << "[" << commandData.arguments[ i ].arraySize << "]";
							}
							if( lastArgument == i )
							{
								std::map<std::string, FlagData>::const_iterator flagIt = vkData.flags.find( commandData.arguments[ i ].pureType );
								if( flagIt != vkData.flags.end() )
								{
									std::list<DependencyData>::const_iterator depIt = std::find_if( vkData.dependencies.begin(), vkData.dependencies.end(), [ &flagIt ]( DependencyData const& dd ) { return( dd.name == flagIt->first ); } );
									assert( depIt != vkData.dependencies.end() );
									assert( depIt->dependencies.size() == 1 );
									std::map<std::string, EnumData>::const_iterator enumIt = vkData.enums.find( *depIt->dependencies.begin() );
									assert( enumIt != vkData.enums.end() );
									if( enumIt->second.members.empty() )
									{
										ofs << " = " << commandData.arguments[ i ].pureType << "()";
									}
								}
							}
						}
						else
						{
							assert( commandData.arguments[ i ].type[ pos ] == '*' );
							if( commandData.arguments[ i ].optional )
							{
								ofs << "Optional<" << trimEnd( commandData.arguments[ i ].type.substr( 0, pos ) ) << "> " << reduceName( commandData.arguments[ i ].name ) << " = nullptr";
							}
							else if( commandData.arguments[ i ].type.find( "char" ) == std::string::npos )
							{
								ofs << trimEnd( commandData.arguments[ i ].type.substr( 0, pos ) ) << " & " << reduceName( commandData.arguments[ i ].name );
							}
							else
							{
								ofs << "const std::string & " << reduceName( commandData.arguments[ i ].name );
							}
						}
					}
					else
					{
						bool optional = commandData.arguments[ i ].optional && ( ( it == vectorParameters.end() ) || ( it->second == ~0 ) );
						assert( pos != std::string::npos );
						assert( commandData.arguments[ i ].type[ pos ] == '*' );
						if( commandData.arguments[ i ].type.find( "char" ) != std::string::npos )
						{
							if( optional )
							{
								ofs << "Optional<const std::string> " << reduceName( commandData.arguments[ i ].name ) << " = nullptr";
							}
							else
							{
								ofs << "const std::string & " << reduceName( commandData.arguments[ i ].name );
							}
						}
						else
						{
							assert( !optional );
							bool isConst = ( commandData.arguments[ i ].type.find( "const" ) != std::string::npos );
							ofs << "ArrayProxy<" << ( ( templateIndex == i ) ? ( isConst ? "const T" : "T" ) : trimEnd( commandData.arguments[ i ].type.substr( 0, pos ) ) ) << "> " << reduceName( commandData.arguments[ i ].name );
						}
					}
					argEncountered = true;
				}
			}
			ofs << " ";
		}
		ofs << ")";
		if( commandData.handleCommand )
		{
			ofs << " const";
		}
		ofs << std::endl;
	}

	void CppGenerator::writeMemberData( std::ofstream & ofs, MemberData const& memberData, std::set<std::string> const& vkTypes )
	{
		if( vkTypes.find( memberData.pureType ) != vkTypes.end() )
		{
			if( memberData.type.back() == '*' )
			{
				ofs << "reinterpret_cast<";
				if( memberData.type.find( "const" ) == 0 )
				{
					ofs << "const ";
				}
				ofs << "Vk" << memberData.pureType << '*';
			}
			else
			{
				ofs << "static_cast<Vk" << memberData.pureType;
			}
			ofs << ">( " << memberData.name << " )";
		}
		else
		{
			ofs << memberData.name;
		}
	}

	void CppGenerator::writeStructConstructor( std::ofstream & ofs, std::string const& name, StructData const& structData, std::set<std::string> const& vkTypes, std::map<std::string, std::string> const& defaultValues )
	{
		// the constructor with all the elements as arguments, with defaults
		ofs << "    " << name << "( ";
		bool listedArgument = false;
		for( size_t i = 0; i < structData.members.size(); i++ )
		{
			if( listedArgument )
			{
				ofs << ", ";
			}
			if( ( structData.members[ i ].name != "pNext" ) && ( structData.members[ i ].name != "sType" ) )
			{
				std::map<std::string, std::string>::const_iterator defaultIt = defaultValues.find( structData.members[ i ].pureType );
				assert( defaultIt != defaultValues.end() );
				if( structData.members[ i ].arraySize.empty() )
				{
					ofs << structData.members[ i ].type + " " + structData.members[ i ].name << "_ = " << ( structData.members[ i ].type.back() == '*' ? "nullptr" : defaultIt->second );
				}
				else
				{
					ofs << "std::array<" + structData.members[ i ].type + "," + structData.members[ i ].arraySize + "> const& " + structData.members[ i ].name << "_ = { " << defaultIt->second;
					size_t n = atoi( structData.members[ i ].arraySize.c_str() );
					assert( 0 < n );
					for( size_t j = 1; j < n; j++ )
					{
						ofs << ", " << defaultIt->second;
					}
					ofs << " }";
				}
				listedArgument = true;
			}
		}
		ofs << " )" << std::endl;

		// copy over the simple arguments
		bool firstArgument = true;
		for( size_t i = 0; i < structData.members.size(); i++ )
		{
			if( structData.members[ i ].arraySize.empty() )
			{
				ofs << "      " << ( firstArgument ? ":" : "," ) << " " << structData.members[ i ].name << "( ";
				if( structData.members[ i ].name == "pNext" )
				{
					ofs << "nullptr";
				}
				else if( structData.members[ i ].name == "sType" )
				{
					ofs << "StructureType::e" << name;
				}
				else
				{
					ofs << structData.members[ i ].name << "_";
				}
				ofs << " )" << std::endl;
				firstArgument = false;
			}
		}

		// the body of the constructor, copying over data from argument list into wrapped struct
		ofs << "    {" << std::endl;
		for( size_t i = 0; i < structData.members.size(); i++ )
		{
			if( !structData.members[ i ].arraySize.empty() )
			{
				ofs << "      memcpy( &" << structData.members[ i ].name << ", " << structData.members[ i ].name << "_.data(), " << structData.members[ i ].arraySize << " * sizeof( " << structData.members[ i ].type << " ) );" << std::endl;
			}
		}
		ofs << "    }" << std::endl
			<< std::endl;

		// the copy constructor from a native struct (Vk...)
		ofs << "    " << name << "( Vk" << name << " const & rhs )" << std::endl
			<< "    {" << std::endl
			<< "      memcpy( this, &rhs, sizeof(" << name << ") );" << std::endl
			<< "    }" << std::endl
			<< std::endl;

		// the assignment operator from a native sturct (Vk...)
		ofs << "    " << name << "& operator=( Vk" << name << " const & rhs )" << std::endl
			<< "    {" << std::endl
			<< "      memcpy( this, &rhs, sizeof(" << name << ") );" << std::endl
			<< "      return *this;" << std::endl
			<< "    }" << std::endl
			<< std::endl;
	}

	void CppGenerator::writeStructSetter( std::ofstream & ofs, std::string const& name, MemberData const& memberData, std::set<std::string> const& vkTypes )
	{
		ofs << "    " << name << "& set" << static_cast<char>( toupper( memberData.name[ 0 ] ) ) << memberData.name.substr( 1 ) << "( ";
		if( memberData.arraySize.empty() )
		{
			ofs << memberData.type << " ";
		}
		else
		{
			ofs << "std::array<" << memberData.type << "," << memberData.arraySize << "> ";
		}
		ofs << memberData.name << "_ )" << std::endl
			<< "    {" << std::endl;
		if( !memberData.arraySize.empty() )
		{
			ofs << "      memcpy( &" << memberData.name << ", " << memberData.name << "_.data(), " << memberData.arraySize << " * sizeof( " << memberData.type << " ) )";
		}
		else
		{
			ofs << "      " << memberData.name << " = " << memberData.name << "_";
		}
		ofs << ";" << std::endl
			<< "      return *this;" << std::endl
			<< "    }" << std::endl
			<< std::endl;
	}

	void CppGenerator::writeTypeCommand( std::ofstream & ofs, VkData const& vkData, DependencyData const& dependencyData )
	{
		assert( vkData.commands.find( dependencyData.name ) != vkData.commands.end() );
		CommandData const& commandData = vkData.commands.find( dependencyData.name )->second;
		if( !commandData.handleCommand )
		{
			writeTypeCommandStandard( ofs, "  ", dependencyData.name, dependencyData, commandData, vkData.vkTypes );

			ofs << std::endl
				<< "#ifndef VKCPP_DISABLE_ENHANCED_MODE" << std::endl;
			writeTypeCommandEnhanced( ofs, vkData, "  ", "", dependencyData.name, dependencyData, commandData );
			ofs << "#endif /*VKCPP_DISABLE_ENHANCED_MODE*/" << std::endl
				<< std::endl;
		}
	}

	void CppGenerator::writeTypeCommandEnhanced( std::ofstream & ofs, VkData const& vkData, std::string const& indentation, std::string const& className, std::string const& functionName, DependencyData const& dependencyData, CommandData const& commandData )
	{
		enterProtect( ofs, commandData.protect );
		std::map<size_t, size_t> vectorParameters = getVectorParameters( commandData );
		size_t returnIndex = findReturnIndex( commandData, vectorParameters );
		size_t templateIndex = findTemplateIndex( commandData, vectorParameters );
		std::map<size_t, size_t>::const_iterator returnVector = vectorParameters.find( returnIndex );
		std::string returnType = determineReturnType( commandData, returnIndex, returnVector != vectorParameters.end() );

		writeFunctionHeader( ofs, vkData, indentation, returnType, functionName, commandData, returnIndex, templateIndex, vectorParameters );
		writeFunctionBody( ofs, indentation, className, functionName, returnType, templateIndex, dependencyData, commandData, vkData.vkTypes, returnIndex, vectorParameters );
		leaveProtect( ofs, commandData.protect );
	}

	void CppGenerator::writeTypeCommandStandard( std::ofstream & ofs, std::string const& indentation, std::string const& functionName, DependencyData const& dependencyData, CommandData const& commandData, std::set<std::string> const& vkTypes )
	{
		enterProtect( ofs, commandData.protect );
		ofs << indentation;
		if( !commandData.handleCommand )
		{
			ofs << "inline ";
		}
		ofs << commandData.returnType << " " << functionName << "( ";
		bool argEncountered = false;
		for( size_t i = commandData.handleCommand ? 1 : 0; i < commandData.arguments.size(); i++ )
		{
			if( argEncountered )
			{
				ofs << ", ";
			}
			ofs << commandData.arguments[ i ].type << " " << commandData.arguments[ i ].name;
			if( !commandData.arguments[ i ].arraySize.empty() )
			{
				ofs << "[" << commandData.arguments[ i ].arraySize << "]";
			}
			argEncountered = true;
		}
		ofs << " )";
		if( commandData.handleCommand )
		{
			ofs << " const";
		}
		ofs << std::endl
			<< indentation << "{" << std::endl
			<< indentation << "  ";
		bool castReturn = false;
		if( commandData.returnType != "void" )
		{
			ofs << "return ";
			castReturn = ( vkTypes.find( commandData.returnType ) != vkTypes.end() );
			if( castReturn )
			{
				ofs << "static_cast<" << commandData.returnType << ">( ";
			}
		}

		std::string callName( dependencyData.name );
		assert( islower( callName[ 0 ] ) );
		callName[ 0 ] = toupper( callName[ 0 ] );

		ofs << "vk" << callName << "( ";
		if( commandData.handleCommand )
		{
			ofs << "m_" << commandData.arguments[ 0 ].name;
		}
		argEncountered = false;
		for( size_t i = commandData.handleCommand ? 1 : 0; i < commandData.arguments.size(); i++ )
		{
			if( 0 < i )
			{
				ofs << ", ";
			}
			writeMemberData( ofs, commandData.arguments[ i ], vkTypes );
		}
		ofs << " )";
		if( castReturn )
		{
			ofs << " )";
		}
		ofs << ";" << std::endl
			<< indentation << "}" << std::endl;
		leaveProtect( ofs, commandData.protect );
	}

	void CppGenerator::writeTypeEnum( std::ofstream & ofs, DependencyData const& dependencyData, EnumData const& enumData )
	{
		enterProtect( ofs, enumData.protect );
		ofs << "  enum class " << dependencyData.name << std::endl
			<< "  {" << std::endl;
		for( size_t i = 0; i < enumData.members.size(); i++ )
		{
			ofs << "    " << enumData.members[ i ].name << " = " << enumData.members[ i ].value;
			if( i < enumData.members.size() - 1 )
			{
				ofs << ",";
			}
			ofs << std::endl;
		}
		ofs << "  };" << std::endl;
		leaveProtect( ofs, enumData.protect );
		ofs << std::endl;
	}

	void CppGenerator::writeEnumsToString( std::ofstream & ofs, DependencyData const& dependencyData, EnumData const& enumData )
	{
		enterProtect( ofs, enumData.protect );
		ofs << "  inline std::string to_string(" << dependencyData.name << ( enumData.members.empty() ? ")" : " value)" ) << std::endl
			<< "  {" << std::endl;
		if( enumData.members.empty() )
		{
			ofs << "    return \"(void)\";" << std::endl;
		}
		else
		{
			ofs << "    switch (value)" << std::endl
				<< "    {" << std::endl;
			for( auto itMember = enumData.members.begin(); itMember != enumData.members.end(); ++itMember )
			{
				ofs << "    case " << dependencyData.name << "::" << itMember->name << ": return \"" << itMember->name.substr( 1 ) << "\";" << std::endl;
			}
			ofs << "    default: return \"invalid\";" << std::endl
				<< "    }" << std::endl;
		}
		ofs << "  }" << std::endl;
		leaveProtect( ofs, enumData.protect );
		ofs << std::endl;
	}

	void CppGenerator::writeFlagsToString( std::ofstream & ofs, DependencyData const& dependencyData, EnumData const &enumData )
	{
		enterProtect( ofs, enumData.protect );
		std::string enumPrefix = *dependencyData.dependencies.begin() + "::";
		ofs << "  inline std::string to_string(" << dependencyData.name << ( enumData.members.empty() ? ")" : " value)" ) << std::endl
			<< "  {" << std::endl;
		if( enumData.members.empty() )
		{
			ofs << "    return \"{}\";" << std::endl;
		}
		else
		{
			ofs << "    if (!value) return \"{}\";" << std::endl
				<< "    std::string result;" << std::endl;

			for( auto itMember = enumData.members.begin(); itMember != enumData.members.end(); ++itMember )
			{
				ofs << "    if (value & " << enumPrefix + itMember->name << ") result += \"" << itMember->name.substr( 1 ) << " | \";" << std::endl;
			}
			ofs << "    return \"{\" + result.substr(0, result.size() - 3) + \"}\";" << std::endl;
		}
		ofs << "  }" << std::endl;
		leaveProtect( ofs, enumData.protect );
		ofs << std::endl;
	}

	void CppGenerator::writeEnumsToString( std::ofstream & ofs, VkData const& vkData )
	{
		for( auto it = vkData.dependencies.begin(); it != vkData.dependencies.end(); ++it )
		{
			switch( it->category )
			{
				case DependencyData::Category::ENUM:
					assert( vkData.enums.find( it->name ) != vkData.enums.end() );
					writeEnumsToString( ofs, *it, vkData.enums.find( it->name )->second );
					break;
				case DependencyData::Category::FLAGS:
					writeFlagsToString( ofs, *it, vkData.enums.find( *it->dependencies.begin() )->second );
					break;
			}
		}
	}

	void CppGenerator::writeTypeFlags( std::ofstream & ofs, DependencyData const& dependencyData, FlagData const& flagData )
	{
		assert( dependencyData.dependencies.size() == 1 );
		enterProtect( ofs, flagData.protect );
		ofs << "  using " << dependencyData.name << " = Flags<" << *dependencyData.dependencies.begin() << ", Vk" << dependencyData.name << ">;" << std::endl
			<< std::endl
			<< "  inline " << dependencyData.name << " operator|( " << *dependencyData.dependencies.begin() << " bit0, " << *dependencyData.dependencies.begin() << " bit1 )" << std::endl
			<< "  {" << std::endl
			<< "    return " << dependencyData.name << "( bit0 ) | bit1;" << std::endl
			<< "  }" << std::endl;
		leaveProtect( ofs, flagData.protect );
		ofs << std::endl;
	}

	void CppGenerator::writeTypeHandle( std::ofstream & ofs, VkData const& vkData, DependencyData const& dependencyData, HandleData const& handle, std::list<DependencyData> const& dependencies )
	{
		std::string memberName = dependencyData.name;
		assert( isupper( memberName[ 0 ] ) );
		memberName[ 0 ] = tolower( memberName[ 0 ] );

		ofs << "  class " << dependencyData.name << std::endl
			<< "  {" << std::endl
			<< "  public:" << std::endl
			<< "    " << dependencyData.name << "()" << std::endl
			<< "      : m_" << memberName << "(VK_NULL_HANDLE)" << std::endl
			<< "    {}" << std::endl
			<< std::endl
			<< "#if defined(VK_CPP_TYPESAFE_CONVERSION)" << std::endl
			// construct from native handle
			<< "    " << dependencyData.name << "(Vk" << dependencyData.name << " " << memberName << ")" << std::endl
			<< "       : m_" << memberName << "(" << memberName << ")" << std::endl
			<< "    {}" << std::endl
			<< std::endl
			// assignment from native handle
			<< "    " << dependencyData.name << "& operator=(Vk" << dependencyData.name << " " << memberName << ")" << std::endl
			<< "    {" << std::endl
			<< "      m_" << memberName << " = " << memberName << ";" << std::endl
			<< "      return *this;" << std::endl
			<< "    }" << std::endl
			<< "#endif\n"
			<< std::endl;

		if( !handle.commands.empty() )
		{
			for( size_t i = 0; i < handle.commands.size(); i++ )
			{
				std::string commandName = handle.commands[ i ];
				std::map<std::string, CommandData>::const_iterator cit = vkData.commands.find( commandName );
				assert( ( cit != vkData.commands.end() ) && cit->second.handleCommand );
				std::list<DependencyData>::const_iterator dep = std::find_if( dependencies.begin(), dependencies.end(), [ commandName ]( DependencyData const& dd ) { return dd.name == commandName; } );
				assert( dep != dependencies.end() );
				std::string className = dependencyData.name;
				std::string functionName = determineFunctionName( dep->name, cit->second );

				bool hasPointers = hasPointerArguments( cit->second );
				if( !hasPointers )
				{
					ofs << "#ifdef VKCPP_DISABLE_ENHANCED_MODE" << std::endl;
				}
				writeTypeCommandStandard( ofs, "    ", functionName, *dep, cit->second, vkData.vkTypes );
				if( !hasPointers )
				{
					ofs << "#endif /*!VKCPP_DISABLE_ENHANCED_MODE*/" << std::endl;
				}

				ofs << std::endl
					<< "#ifndef VKCPP_DISABLE_ENHANCED_MODE" << std::endl;
				writeTypeCommandEnhanced( ofs, vkData, "    ", className, functionName, *dep, cit->second );
				ofs << "#endif /*VKCPP_DISABLE_ENHANCED_MODE*/" << std::endl;

				if( i < handle.commands.size() - 1 )
				{
					ofs << std::endl;
				}
			}
			ofs << std::endl;
		}
		ofs << "#if !defined(VK_CPP_TYPESAFE_CONVERSION)" << std::endl
			<< "    explicit" << std::endl
			<< "#endif" << std::endl
			<< "    operator Vk" << dependencyData.name << "() const" << std::endl
			<< "    {" << std::endl
			<< "      return m_" << memberName << ";" << std::endl
			<< "    }" << std::endl
			<< std::endl
			<< "    explicit operator bool() const" << std::endl
			<< "    {" << std::endl
			<< "      return m_" << memberName << " != VK_NULL_HANDLE;" << std::endl
			<< "    }" << std::endl
			<< std::endl
			<< "    bool operator!() const" << std::endl
			<< "    {" << std::endl
			<< "      return m_" << memberName << " == VK_NULL_HANDLE;" << std::endl
			<< "    }" << std::endl
			<< std::endl
			<< "  private:" << std::endl
			<< "    Vk" << dependencyData.name << " m_" << memberName << ";" << std::endl
			<< "  };" << std::endl
	#if 1
			<< "  static_assert( sizeof( " << dependencyData.name << " ) == sizeof( Vk" << dependencyData.name << " ), \"handle and wrapper have different size!\" );" << std::endl
	#endif
			<< std::endl;
	}

	void CppGenerator::writeTypeScalar( std::ofstream & ofs, DependencyData const& dependencyData )
	{
		assert( dependencyData.dependencies.size() == 1 );
		ofs << "  using " << dependencyData.name << " = " << *dependencyData.dependencies.begin() << ";" << std::endl
			<< std::endl;
	}

	void CppGenerator::writeTypeStruct( std::ofstream & ofs, VkData const& vkData, DependencyData const& dependencyData, std::map<std::string, std::string> const& defaultValues )
	{
		std::map<std::string, StructData>::const_iterator it = vkData.structs.find( dependencyData.name );
		assert( it != vkData.structs.end() );

		enterProtect( ofs, it->second.protect );
		ofs << "  struct " << dependencyData.name << std::endl
			<< "  {" << std::endl;

		// only structs that are not returnedOnly get a constructor!
		if( !it->second.returnedOnly )
		{
			writeStructConstructor( ofs, dependencyData.name, it->second, vkData.vkTypes, defaultValues );
		}

		// create the setters
		if( !it->second.returnedOnly )
		{
			for( size_t i = 0; i < it->second.members.size(); i++ )
			{
				writeStructSetter( ofs, dependencyData.name, it->second.members[ i ], vkData.vkTypes );
			}
		}

		// the cast-operator to the wrapped struct
		ofs << "    operator const Vk" << dependencyData.name << "&() const" << std::endl
			<< "    {" << std::endl
			<< "      return *reinterpret_cast<const Vk" << dependencyData.name << "*>(this);" << std::endl
			<< "    }" << std::endl
			<< std::endl;

		// the member variables
		for( size_t i = 0; i < it->second.members.size(); i++ )
		{
			if( it->second.members[ i ].type == "StructureType" )
			{
				assert( ( i == 0 ) && ( it->second.members[ i ].name == "sType" ) );
				ofs << "  private:" << std::endl
					<< "    StructureType sType;" << std::endl
					<< std::endl
					<< "  public:" << std::endl;
			}
			else
			{
				ofs << "    " << it->second.members[ i ].type << " " << it->second.members[ i ].name;
				if( !it->second.members[ i ].arraySize.empty() )
				{
					ofs << "[" << it->second.members[ i ].arraySize << "]";
				}
				ofs << ";" << std::endl;
			}
		}
		ofs << "  };" << std::endl;
	#if 1
		ofs << "  static_assert( sizeof( " << dependencyData.name << " ) == sizeof( Vk" << dependencyData.name << " ), \"struct and wrapper have different size!\" );" << std::endl;
	#endif
		leaveProtect( ofs, it->second.protect );
		ofs << std::endl;
	}

	void CppGenerator::writeTypeUnion( std::ofstream & ofs, VkData const& vkData, DependencyData const& dependencyData, StructData const& unionData, std::map<std::string, std::string> const& defaultValues )
	{
		std::ostringstream oss;
		ofs << "  union " << dependencyData.name << std::endl
			<< "  {" << std::endl;

		for( size_t i = 0; i < unionData.members.size(); i++ )
		{
			// one constructor per union element
			ofs << "    " << dependencyData.name << "( ";
			if( unionData.members[ i ].arraySize.empty() )
			{
				ofs << unionData.members[ i ].type << " ";
			}
			else
			{
				ofs << "const std::array<" << unionData.members[ i ].type << "," << unionData.members[ i ].arraySize << ">& ";
			}
			ofs << unionData.members[ i ].name << "_";

			// just the very first constructor gets default arguments
			if( i == 0 )
			{
				std::map<std::string, std::string>::const_iterator it = defaultValues.find( unionData.members[ i ].pureType );
				assert( it != defaultValues.end() );
				if( unionData.members[ i ].arraySize.empty() )
				{
					ofs << " = " << it->second;
				}
				else
				{
					ofs << " = { " << it->second << " }";
				}
			}
			ofs << " )" << std::endl
				<< "    {" << std::endl
				<< "      ";
			if( unionData.members[ i ].arraySize.empty() )
			{
				ofs << unionData.members[ i ].name << " = " << unionData.members[ i ].name << "_";
			}
			else
			{
				ofs << "memcpy( &" << unionData.members[ i ].name << ", " << unionData.members[ i ].name << "_.data(), " << unionData.members[ i ].arraySize << " * sizeof( " << unionData.members[ i ].type << " ) )";
			}
			ofs << ";" << std::endl
				<< "    }" << std::endl
				<< std::endl;
		}

		for( size_t i = 0; i < unionData.members.size(); i++ )
		{
			// one setter per union element
			assert( !unionData.returnedOnly );
			writeStructSetter( ofs, dependencyData.name, unionData.members[ i ], vkData.vkTypes );
		}

		// the implicit cast operator to the native type
		ofs << "    operator Vk" << dependencyData.name << " const& () const" << std::endl
			<< "    {" << std::endl
			<< "      return *reinterpret_cast<const Vk" << dependencyData.name << "*>(this);" << std::endl
			<< "    }" << std::endl
			<< std::endl;

		// the union member variables
		// if there's at least one Vk... type in this union, check for unrestricted unions support
		bool needsUnrestrictedUnions = false;
		for( size_t i = 0; i < unionData.members.size() && !needsUnrestrictedUnions; i++ )
		{
			needsUnrestrictedUnions = ( vkData.vkTypes.find( unionData.members[ i ].type ) != vkData.vkTypes.end() );
		}
		if( needsUnrestrictedUnions )
		{
			ofs << "#ifdef VK_CPP_HAS_UNRESTRICTED_UNIONS" << std::endl;
			for( size_t i = 0; i < unionData.members.size(); i++ )
			{
				ofs << "    " << unionData.members[ i ].type << " " << unionData.members[ i ].name;
				if( !unionData.members[ i ].arraySize.empty() )
				{
					ofs << "[" << unionData.members[ i ].arraySize << "]";
				}
				ofs << ";" << std::endl;
			}
			ofs << "#else" << std::endl;
		}
		for( size_t i = 0; i < unionData.members.size(); i++ )
		{
			ofs << "    ";
			if( vkData.vkTypes.find( unionData.members[ i ].type ) != vkData.vkTypes.end() )
			{
				ofs << "Vk";
			}
			ofs << unionData.members[ i ].type << " " << unionData.members[ i ].name;
			if( !unionData.members[ i ].arraySize.empty() )
			{
				ofs << "[" << unionData.members[ i ].arraySize << "]";
			}
			ofs << ";" << std::endl;
		}
		if( needsUnrestrictedUnions )
		{
			ofs << "#endif  // VK_CPP_HAS_UNRESTRICTED_UNIONS" << std::endl;
		}
		ofs << "  };" << std::endl
			<< std::endl;
	}

	void CppGenerator::writeTypes( std::ofstream & ofs, VkData const& vkData, std::map<std::string, std::string> const& defaultValues )
	{
		for( std::list<DependencyData>::const_iterator it = vkData.dependencies.begin(); it != vkData.dependencies.end(); ++it )
		{
			switch( it->category )
			{
				case DependencyData::Category::COMMAND:
					writeTypeCommand( ofs, vkData, *it );
					break;
				case DependencyData::Category::ENUM:
					assert( vkData.enums.find( it->name ) != vkData.enums.end() );
					writeTypeEnum( ofs, *it, vkData.enums.find( it->name )->second );
					break;
				case DependencyData::Category::FLAGS:
					assert( vkData.flags.find( it->name ) != vkData.flags.end() );
					writeTypeFlags( ofs, *it, vkData.flags.find( it->name )->second );
					break;
				case DependencyData::Category::FUNC_POINTER:
				case DependencyData::Category::REQUIRED:
					// skip FUNC_POINTER and REQUIRED, they just needed to be in the dependencies list to resolve dependencies
					break;
				case DependencyData::Category::HANDLE:
					assert( vkData.handles.find( it->name ) != vkData.handles.end() );
					writeTypeHandle( ofs, vkData, *it, vkData.handles.find( it->name )->second, vkData.dependencies );
					break;
				case DependencyData::Category::SCALAR:
					writeTypeScalar( ofs, *it );
					break;
				case DependencyData::Category::STRUCT:
					writeTypeStruct( ofs, vkData, *it, defaultValues );
					break;
				case DependencyData::Category::UNION:
					assert( vkData.structs.find( it->name ) != vkData.structs.end() );
					writeTypeUnion( ofs, vkData, *it, vkData.structs.find( it->name )->second, defaultValues );
					break;
				default:
					assert( false );
					break;
			}
		}
	}

	void CppGenerator::writeVersionCheck( std::ofstream & ofs, std::string const& version )
	{
		ofs << "static_assert( VK_HEADER_VERSION == " << version << " , \"Wrong VK_HEADER_VERSION!\" );" << std::endl
			<< std::endl;
	}

	void CppGenerator::writeTypesafeCheck( std::ofstream & ofs, std::string const& typesafeCheck )
	{
		ofs << "// 32-bit vulkan is not typesafe for handles, so don't allow copy constructors on this platform by default." << std::endl
			<< "// To enable this feature on 32-bit platforms please define VK_CPP_TYPESAFE_CONVERSION" << std::endl
			<< typesafeCheck << std::endl
			<< "#define VK_CPP_TYPESAFE_CONVERSION 1" << std::endl
			<< "#endif" << std::endl
			<< std::endl;
	}
} // end of vk namespace
