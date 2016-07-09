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
#include "VkSpecParser.h"
#include "StringsHelper.h"
#include <cassert>
#include <iostream>
#include <algorithm>

namespace vk
{
	void EnumData::addEnum( std::string const & name, std::string const& tag, bool appendTag )
	{
		assert( tag.empty() || ( name.find( tag ) != std::string::npos ) );
		members.push_back( NameValue() );
		members.back().name = "e" + StringsHelper::toCamelCase( StringsHelper::strip( name, prefix, tag ) );
		members.back().value = name;
		if( !postfix.empty() )
		{
			size_t pos = members.back().name.find( postfix );
			if( pos != std::string::npos )
				members.back().name.erase( pos );
		}
		if( appendTag && !tag.empty() )
			members.back().name += tag;
	}

	SpecParser::SpecParser()
	{}
	//--------------------------------------------------------------------------
	SpecData* SpecParser::parse( const std::string& filename ) const
	{
		SpecData* vkData = 0;

		try
		{
			tinyxml2::XMLDocument doc;
			std::cout << "Loading vk.xml from " << filename << std::endl;

			tinyxml2::XMLError error = doc.LoadFile( filename.c_str() );
			if( error != tinyxml2::XML_SUCCESS )
			{
				std::cerr << "VkSpecParser: failed to load file " << filename
						  << " . Error code: " << error << std::endl;
				return 0;
			}

			auto registryElement = doc.FirstChildElement();
			assert( strcmp( registryElement->Value(), "registry" ) == 0 );
			assert( !registryElement->NextSiblingElement() );

			vkData = new SpecData;
			auto child = registryElement->FirstChildElement();
			do
			{
				assert( child->Value() );
				const std::string value = child->Value();
				if( value == "commands" )
					readCommands( child, vkData );

				else if( value == "comment" )
					readComment( child, vkData->vulkanLicenseHeader );

				else if( value == "enums" )
					readEnums( child, vkData );

				else if( value == "extensions" )
					readExtensions( child, vkData );

				else if( value == "tags" )
					readTags( child, vkData->tags );

				else if( value == "types" )
					readTypes( child, vkData );
				else
					assert( ( value == "feature" ) || ( value == "vendorids" ) );
			} while( child = child->NextSiblingElement() );
		}
		catch( const std::exception& e )
		{
			std::cerr << "VkSpecParser caught an exception: " << e.what() << std::endl;
			return 0;
		}
		catch( ... )
		{
			std::cerr << "VkSpecParser caught an unknown exception" << std::endl;
			return 0;
		}

		return vkData;
	}
	//--------------------------------------------------------------------------
	std::string SpecParser::getEnumName( std::string const& name ) const
	{
		return StringsHelper::strip( name, "Vk" );
	}
	//--------------------------------------------------------------------------
	std::string SpecParser::stripCommand( std::string const& value ) const
	{
		std::string stripped = StringsHelper::strip( value, "vk" );
		assert( isupper( stripped[ 0 ] ) );
		stripped[ 0 ] = tolower( stripped[ 0 ] );
		return stripped;
	}
	//--------------------------------------------------------------------------
	std::string SpecParser::findTag( std::string const& name, std::set<std::string> const& tags ) const
	{
		for( auto& it : tags )
		{
			size_t pos = name.find( it );
			if( pos != std::string::npos && ( pos == name.length() - it.length() ) )
				return it;
		}
		return "";
	}
	//--------------------------------------------------------------------------
	std::string SpecParser::extractTag( std::string const& name ) const
	{
		// the name is supposed to look like: VK_<tag>_<other>
		size_t start = name.find( '_' );
		assert( start != std::string::npos );

		size_t end = name.find( '_', start + 1 );
		assert( end != std::string::npos );

		return name.substr( start + 1, end - start - 1 );
	}
	//--------------------------------------------------------------------------
	std::string SpecParser::generateEnumNameForFlags( std::string const& name ) const
	{
		std::string generatedName = name;
		size_t pos = generatedName.rfind( "Flags" );
		assert( pos != std::string::npos );
		generatedName.replace( pos, 5, "FlagBits" );
		return generatedName;
	}
	//--------------------------------------------------------------------------
	void SpecParser::readCommands( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto child = element->FirstChildElement();
		assert( child );
		do
		{
			assert( strcmp( child->Value(), "command" ) == 0 );
			readCommandsCommand( child, vkData );
		} while( child = child->NextSiblingElement() );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readCommandsCommand( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto child = element->FirstChildElement();
		assert( child && strcmp( child->Value(), "proto" ) == 0 );

		std::map<std::string, CommandData>::iterator it = readCommandProto( child, vkData );

		if( element->Attribute( "successcodes" ) )
		{
			std::string successCodes = element->Attribute( "successcodes" );
			size_t start = 0, end;
			do
			{
				end = successCodes.find( ',', start );
				std::string code = successCodes.substr( start, end - start );
				std::string tag = findTag( code, vkData->tags );
				it->second.successCodes.push_back( "e" + StringsHelper::toCamelCase( StringsHelper::strip( code, "VK_", tag ) ) + tag );
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
				it->second.twoStep |= readCommandParam( child, vkData->dependencies.back(), it->second.arguments );
			else
				assert( value == "implicitexternsyncparams" || value == "validity" );
		}

		// HACK: the current vk.xml misses to specify <optional="false,true"> on param pSparseMemoryRequirementCount on command vkGetImageSparseMemoryRequirements!
		if( it->first == "getImageSparseMemoryRequirements" )
			it->second.twoStep = true;

		assert( !it->second.arguments.empty() );
		std::map<std::string, HandleData>::iterator hit = vkData->handles.find( it->second.arguments[ 0 ].pureType );
		if( hit != vkData->handles.end() )
		{
			hit->second.commands.push_back( it->first );
			it->second.handleCommand = true;
			DependencyData const& dep = vkData->dependencies.back();
			std::list<DependencyData>::iterator dit = std::find_if( vkData->dependencies.begin(), vkData->dependencies.end(), [ hit ]( DependencyData const& dd ) { return dd.name == hit->first; } );
			for( std::set<std::string>::const_iterator depit = dep.dependencies.begin(); depit != dep.dependencies.end(); ++depit )
			{
				if( *depit != hit->first )
					dit->dependencies.insert( *depit );
			}
		}
	}
	//--------------------------------------------------------------------------
	std::map<std::string, CommandData>::iterator SpecParser::readCommandProto(
			tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto typeElement = element->FirstChildElement();
		assert( typeElement && ( strcmp( typeElement->Value(), "type" ) == 0 ) );
		auto nameElement = typeElement->NextSiblingElement();
		assert( nameElement && ( strcmp( nameElement->Value(), "name" ) == 0 ) );
		assert( !nameElement->NextSiblingElement() );

		std::string type = StringsHelper::strip( typeElement->GetText(), "Vk" );
		std::string name = stripCommand( nameElement->GetText() );

		vkData->dependencies.push_back( DependencyData( DependencyData::Category::COMMAND, name ) );
		assert( vkData->commands.find( name ) == vkData->commands.end() );
		std::map<std::string, CommandData>::iterator it = vkData->commands.insert( std::make_pair( name, CommandData() ) ).first;
		it->second.returnType = type;

		return it;
	}
	//--------------------------------------------------------------------------
	bool SpecParser::readCommandParam( tinyxml2::XMLElement* element,
									   DependencyData& typeData,
									   std::vector<MemberData>& arguments ) const
	{
		arguments.push_back( MemberData() );
		MemberData & arg = arguments.back();

		if( element->Attribute( "len" ) )
			arg.len = element->Attribute( "len" );

		auto child = element->FirstChild();
		assert( child );
		if( child->ToText() )
		{
			std::string value = StringsHelper::trimEnd( child->Value() );
			assert( ( value == "const" ) || ( value == "struct" ) );
			arg.type = value + " ";
			child = child->NextSibling();
			assert( child );
		}

		assert( child->ToElement() );
		assert( ( strcmp( child->Value(), "type" ) == 0 ) && child->ToElement() && child->ToElement()->GetText() );
		std::string type = StringsHelper::strip( child->ToElement()->GetText(), "Vk" );
		typeData.dependencies.insert( type );
		arg.type += type;
		arg.pureType = type;

		child = child->NextSibling();
		assert( child );
		if( child->ToText() )
		{
			std::string value = StringsHelper::trimEnd( child->Value() );
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
	//--------------------------------------------------------------------------
	void SpecParser::readComment( tinyxml2::XMLElement* element, std::string& header ) const
	{
		assert( element->GetText() );
		assert( header.empty() );
		header = element->GetText();
		assert( header.find( "\nCopyright" ) == 0 );

		size_t pos = header.find( "\n\n-----" );
		assert( pos != std::string::npos );
		header.erase( pos );

		for( size_t pos = header.find( '\n' ); pos != std::string::npos; pos = header.find( '\n', pos + 1 ) )
			header.replace( pos, 1, "\n// " );

		header += "\n\n// This header is generated from the Khronos Vulkan XML API Registry.";
	}
	//--------------------------------------------------------------------------
	void SpecParser::readEnums( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		assert( element->Attribute( "name" ) );
		std::string name = getEnumName( element->Attribute( "name" ) );
		if( name != "API Constants" )
		{
			vkData->dependencies.push_back( DependencyData( DependencyData::Category::ENUM, name ) );
			std::map<std::string, EnumData>::iterator it = vkData->enums.insert( std::make_pair( name, EnumData() ) ).first;
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
					it->second.prefix = "VK" + StringsHelper::toUpperCase( name.substr( 0, pos ) ) + "_";
					it->second.postfix = "Bit";
				}
				else
					it->second.prefix = "VK" + StringsHelper::toUpperCase( name ) + "_";

				// if the enum name contains a tag remove it from the prefix to generate correct enum value names.
				for( std::set<std::string>::const_iterator tit = vkData->tags.begin(); tit != vkData->tags.end(); ++tit )
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

			assert( vkData->vkTypes.find( name ) == vkData->vkTypes.end() );
			vkData->vkTypes.insert( name );
		}
	}
	//--------------------------------------------------------------------------
	void SpecParser::readEnumsEnum( tinyxml2::XMLElement* element, EnumData& enumData,
									std::string const& tag ) const
	{
		auto child = element->FirstChildElement();
		do
		{
			if( child->Attribute( "name" ) )
				enumData.addEnum( child->Attribute( "name" ), tag, false );

		} while( child = child->NextSiblingElement() );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readExtensions( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto child = element->FirstChildElement();
		assert( child );
		do
		{
			assert( strcmp( child->Value(), "extension" ) == 0 );
			readExtensionsExtension( child, vkData );
		} while( child = child->NextSiblingElement() );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readExtensionsExtension( tinyxml2::XMLElement* element,
											  SpecData* vkData ) const
	{
		assert( element->Attribute( "name" ) );
		std::string tag = extractTag( element->Attribute( "name" ) );
		assert( vkData->tags.find( tag ) != vkData->tags.end() );

		// don't parse disabled extensions
		if( strcmp( element->Attribute( "supported" ), "disabled" ) == 0 )
			return;

		std::string protect;
		if( element->Attribute( "protect" ) )
			protect = element->Attribute( "protect" );

		auto child = element->FirstChildElement();
		assert( child && strcmp( child->Value(), "require" ) == 0 && !child->NextSiblingElement() );
		readExtensionRequire( child, vkData, protect, tag );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readExtensionRequire( tinyxml2::XMLElement* element,
										   SpecData* vkData,
										   std::string const& protect,
										   std::string const& tag ) const
	{
		auto child = element->FirstChildElement();
		do
		{
			std::string value = child->Value();

			if( value == "command" )
			{
				assert( child->Attribute( "name" ) );
				std::string name = stripCommand( child->Attribute( "name" ) );
				std::map<std::string, CommandData>::iterator cit = vkData->commands.find( name );
				assert( cit != vkData->commands.end() );
				cit->second.protect = protect;
			}
			else if( value == "type" )
			{
				assert( child->Attribute( "name" ) );
				std::string name = StringsHelper::strip( child->Attribute( "name" ), "Vk" );
				std::map<std::string, EnumData>::iterator eit = vkData->enums.find( name );
				if( eit != vkData->enums.end() )
					eit->second.protect = protect;

				else
				{
					std::map<std::string, FlagData>::iterator fit = vkData->flags.find( name );
					if( fit != vkData->flags.end() )
					{
						fit->second.protect = protect;

						// if the enum of this flags is auto-generated, protect it as well
						std::string enumName = generateEnumNameForFlags( name );
						std::map<std::string, EnumData>::iterator eit = vkData->enums.find( enumName );
						assert( eit != vkData->enums.end() );
						if( eit->second.members.empty() )
							eit->second.protect = protect;
					}
					else
					{
						std::map<std::string, ScalarData>::iterator scit = vkData->scalars.find( name );
						if( scit != vkData->scalars.end() )
							scit->second.protect = protect;

						else
						{
							std::map<std::string, StructData>::iterator stit = vkData->structs.find( name );
							assert( stit != vkData->structs.end() && stit->second.protect.empty() );
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
					assert( vkData->enums.find( getEnumName( child->Attribute( "extends" ) ) ) != vkData->enums.end() );
					assert( !!child->Attribute( "bitpos" ) + !!child->Attribute( "offset" ) + !!child->Attribute( "value" ) == 1 );
					vkData->enums[ getEnumName( child->Attribute( "extends" ) ) ].addEnum( child->Attribute( "name" ), child->Attribute( "value" ) ? "" : tag, true );
				}
			}
			else
				assert( value == "usage" );

		} while( child = child->NextSiblingElement() );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTags( tinyxml2::XMLElement* element,
								 std::set<std::string>& tags ) const
	{
		tags.insert( "EXT" );
		tags.insert( "KHR" );
		auto child = element->FirstChildElement();
		do
		{
			assert( child->Attribute( "name" ) );
			tags.insert( child->Attribute( "name" ) );
		} while( child = child->NextSiblingElement() );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypes( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto child = element->FirstChildElement();
		do
		{
			assert( strcmp( child->Value(), "type" ) == 0 );
			std::string type = child->Value();
			assert( type == "type" );
			if( child->Attribute( "category" ) )
			{
				std::string category = child->Attribute( "category" );
				if( category == "basetype" )
					readTypeBasetype( child, vkData->dependencies );

				else if( category == "bitmask" )
					readTypeBitmask( child, vkData );

				else if( category == "define" )
					readTypeDefine( child, vkData );

				else if( category == "funcpointer" )
					readTypeFuncpointer( child, vkData->dependencies );

				else if( category == "handle" )
					readTypeHandle( child, vkData );

				else if( category == "struct" )
					readTypeStruct( child, vkData );

				else if( category == "union" )
					readTypeUnion( child, vkData );

				else
					assert( ( category == "enum" ) || ( category == "include" ) );
			}
			else
			{
				assert( child->Attribute( "requires" ) && child->Attribute( "name" ) );
				vkData->dependencies.push_back( DependencyData( DependencyData::Category::REQUIRED, child->Attribute( "name" ) ) );
			}
		} while( child = child->NextSiblingElement() );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeBasetype( tinyxml2::XMLElement* element,
									   std::list<DependencyData>& dependencies ) const
	{
		auto typeElement = element->FirstChildElement();
		assert( typeElement && strcmp( typeElement->Value(), "type" ) == 0 && typeElement->GetText() );
		std::string type = typeElement->GetText();
		assert( type == "uint32_t" || type == "uint64_t" );

		auto nameElement = typeElement->NextSiblingElement();
		assert( nameElement && strcmp( nameElement->Value(), "name" ) == 0 && nameElement->GetText() );
		std::string name = StringsHelper::strip( nameElement->GetText(), "Vk" );

		// skip "Flags",
		if( name != "Flags" )
		{
			dependencies.push_back( DependencyData( DependencyData::Category::SCALAR, name ) );
			dependencies.back().dependencies.insert( type );
		}
		else
			assert( type == "uint32_t" );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeBitmask( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto typeElement = element->FirstChildElement();
		assert( typeElement && strcmp( typeElement->Value(), "type" ) == 0 &&
				typeElement->GetText() && strcmp( typeElement->GetText(), "VkFlags" ) == 0 );

		//Unused variable
		//std::string type = typeElement->GetText();

		auto nameElement = typeElement->NextSiblingElement();
		assert( nameElement && strcmp( nameElement->Value(), "name" ) == 0 && nameElement->GetText() );
		std::string name = StringsHelper::strip( nameElement->GetText(), "Vk" );

		assert( !nameElement->NextSiblingElement() );

		std::string requires;
		if( element->Attribute( "requires" ) )
			requires = StringsHelper::strip( element->Attribute( "requires" ), "Vk" );

		else
		{
			// Generate FlagBits name
			requires = generateEnumNameForFlags( name );
			vkData->dependencies.push_back( DependencyData( DependencyData::Category::ENUM, requires ) );
			std::map<std::string, EnumData>::iterator it = vkData->enums.insert( std::make_pair( requires, EnumData() ) ).first;
			it->second.bitmask = true;
			vkData->vkTypes.insert( requires );
		}

		vkData->dependencies.push_back( DependencyData( DependencyData::Category::FLAGS, name ) );
		vkData->dependencies.back().dependencies.insert( requires );
		vkData->flags.insert( std::make_pair( name, FlagData() ) );

		assert( vkData->vkTypes.find( name ) == vkData->vkTypes.end() );
		vkData->vkTypes.insert( name );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeDefine( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto child = element->FirstChildElement();
		if( child && strcmp( child->GetText(), "VK_HEADER_VERSION" ) == 0 )
			vkData->version = element->LastChild()->ToText()->Value();

		else if( element->Attribute( "name" ) && strcmp( element->Attribute( "name" ), "VK_DEFINE_NON_DISPATCHABLE_HANDLE" ) == 0 )
		{
			std::string text = element->LastChild()->ToText()->Value();
			size_t start = text.find( '#' );
			size_t end = text.find_first_of( "\r\n", start + 1 );
			vkData->typesafeCheck = text.substr( start, end - start );
		}
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeFuncpointer( tinyxml2::XMLElement* element,
										  std::list<DependencyData>& dependencies ) const
	{
		auto child = element->FirstChildElement();
		assert( child && strcmp( child->Value(), "name" ) == 0 && child->GetText() );
		dependencies.push_back( DependencyData( DependencyData::Category::FUNC_POINTER, child->GetText() ) );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeHandle( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		auto typeElement = element->FirstChildElement();
		assert( typeElement && strcmp( typeElement->Value(), "type" ) == 0 && typeElement->GetText() );
	#ifndef NDEBUG
		std::string type = typeElement->GetText();
		assert( type.find( "VK_DEFINE" ) == 0 );
	#endif

		auto nameElement = typeElement->NextSiblingElement();
		assert( nameElement && strcmp( nameElement->Value(), "name" ) == 0 && nameElement->GetText() );
		std::string name = StringsHelper::strip( nameElement->GetText(), "Vk" );

		vkData->dependencies.push_back( DependencyData( DependencyData::Category::HANDLE, name ) );

		assert( vkData->vkTypes.find( name ) == vkData->vkTypes.end() );
		vkData->vkTypes.insert( name );
		assert( vkData->handles.find( name ) == vkData->handles.end() );
		vkData->handles[ name ];    // add this to the handles map
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeStruct( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		assert( !element->Attribute( "returnedonly" ) ||
				strcmp( element->Attribute( "returnedonly" ), "true" ) == 0 );

		assert( element->Attribute( "name" ) );
		std::string name = StringsHelper::strip( element->Attribute( "name" ), "Vk" );

		if( name == "Rect3D" )
			return;

		vkData->dependencies.push_back( DependencyData( DependencyData::Category::STRUCT, name ) );

		assert( vkData->structs.find( name ) == vkData->structs.end() );
		std::map<std::string, StructData>::iterator it = vkData->structs.insert( std::make_pair( name, StructData() ) ).first;
		it->second.returnedOnly = !!element->Attribute( "returnedonly" );

		auto child = element->FirstChildElement();
		do
		{
			assert( child->Value() );
			std::string value = child->Value();
			if( value == "member" )
				readTypeStructMember( child, it->second.members, vkData->dependencies.back().dependencies );

			else
				assert( value == "validity" );

		} while( child = child->NextSiblingElement() );

		assert( vkData->vkTypes.find( name ) == vkData->vkTypes.end() );
		vkData->vkTypes.insert( name );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeStructMember( tinyxml2::XMLElement* element,
										   std::vector<MemberData>& members,
										   std::set<std::string>& dependencies ) const
	{
		members.push_back( MemberData() );
		MemberData & member = members.back();

		tinyxml2::XMLNode * child = element->FirstChild();
		assert( child );
		if( child->ToText() )
		{
			std::string value = StringsHelper::trimEnd( child->Value() );
			assert( ( value == "const" ) || ( value == "struct" ) );
			member.type = value + " ";
			child = child->NextSibling();
			assert( child );
		}

		assert( child->ToElement() );
		assert( ( strcmp( child->Value(), "type" ) == 0 ) && child->ToElement() && child->ToElement()->GetText() );
		std::string type = StringsHelper::strip( child->ToElement()->GetText(), "Vk" );
		dependencies.insert( type );
		member.type += type;
		member.pureType = type;

		child = child->NextSibling();
		assert( child );
		if( child->ToText() )
		{
			std::string value = StringsHelper::trimEnd( child->Value() );
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
	//--------------------------------------------------------------------------
	void SpecParser::readTypeUnion( tinyxml2::XMLElement* element, SpecData* vkData ) const
	{
		assert( element->Attribute( "name" ) );
		std::string name = StringsHelper::strip( element->Attribute( "name" ), "Vk" );

		vkData->dependencies.push_back( DependencyData( DependencyData::Category::UNION, name ) );

		assert( vkData->structs.find( name ) == vkData->structs.end() );
		auto it = vkData->structs.insert( std::make_pair( name, StructData() ) ).first;

		auto child = element->FirstChildElement();
		do
		{
			assert( strcmp( child->Value(), "member" ) == 0 );
			readTypeUnionMember( child, it->second.members, vkData->dependencies.back().dependencies );
		} while( child = child->NextSiblingElement() );

		assert( vkData->vkTypes.find( name ) == vkData->vkTypes.end() );
		vkData->vkTypes.insert( name );
	}
	//--------------------------------------------------------------------------
	void SpecParser::readTypeUnionMember( tinyxml2::XMLElement* element,
										  std::vector<MemberData>& members,
										  std::set<std::string>& dependencies ) const
	{
		members.push_back( MemberData() );
		MemberData& member = members.back();

		auto child = element->FirstChild();
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
		std::string type = StringsHelper::strip( child->ToElement()->GetText(), "Vk" );
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
}
