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
#include "Strings.h"
#include "StringsHelper.h"

#include <iostream>
#include <cassert>
#include <exception>
#include <algorithm>
#include <sstream>
#include <functional>

namespace vk
{
	int CppGenerator::generate( const Options& opt )
	{
		SpecParser parser;
		auto vkData = parser.parse( opt.inputFile );
		if( !vkData )
			return -1;

		_sortDependencies( vkData->dependencies );

		std::map<std::string, std::string> defaultValues;
		_createDefaults( vkData, defaultValues );

		try
		{
			DualOFStream ofs( opt );
			_indent.setIndentChar( opt.indentChar );
			_indent.setSize( opt.spaceSize );

			ofs << nvidiaLicenseHeader
				<< vkData->vulkanLicenseHeader << std::endl
				<< ( !opt.cmdLine.empty() ? "// Command line options: " + opt.cmdLine + "\n" : "" );

			if( ofs.usingDualStream() )
			{
				if( !opt.pch.empty() )
					ofs.src() << "#include \"" << opt.pch << "\"\n";

				ofs.src() << "#include \"" << ofs.headerFileName() << "\"\n";
			}

			ofs.hdr() << "#ifndef " << opt.includeGuard << std::endl
				<< "#define " << opt.includeGuard << std::endl << std::endl;

			ofs.src() << "#include <cassert>\n"
					  << "#include <cstdint>\n"
					  << "#include <cstring>\n"
					  << "#include <string>\n"
					  << "#include <system_error>\n";

			ofs.hdr() << "#include <array>\n"
					  << "#include <algorithm>\n"
					  << "#include <vulkan/vulkan.h>\n"
					  << "#ifndef VKCPP_DISABLE_ENHANCED_MODE\n"
					  << "#" << _indent + 1 << "include <vector>\n"
					  << "#endif /*VKCPP_DISABLE_ENHANCED_MODE*/\n\n";

			_writeVersionCheck( ofs.src(), vkData->version );
			_writeTypesafeCheck( ofs.hdr(), vkData->typesafeCheck );

			ofs.hdr() << versionCheckHeader;
			ofs << "namespace vk\n"
				<< "{\n";

			ofs.hdr() << flagsHeader
					  << optionalClassHeader
					  << arrayProxyHeader;

			// first of all, write out vk::Result and the exception handling stuff
			auto it = std::find_if(
						vkData->dependencies.begin(),
						vkData->dependencies.end(),
						[]( DependencyData const& dp ) { return dp.name == "Result"; }
			);
			assert( it != vkData->dependencies.end() );
			_writeTypeEnum( ofs.hdr(), *it, vkData->enums.find( it->name )->second );
			_writeEnumsToString( ofs, *it, vkData->enums.find( it->name )->second );
			vkData->dependencies.erase( it );

			ofs.hdr() << exceptionHeader
					  << "} // namespace vk\n\n"
					  << isErrorCode
					  << "\nnamespace vk\n"
					  << "{\n"
					  << resultValueHeader
					  << createResultValueHeader;

			_writeTypes( ofs, vkData, defaultValues );
			_writeEnumsToString( ofs, vkData );

			ofs << "} // namespace vk\n";
			ofs.hdr() << "#endif // " << opt.includeGuard << std::endl;
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
	//--------------------------------------------------------------------------
	void CppGenerator::_enterProtect( DualOFStream& ofs,
									  std::string const& protect ) const
	{
		if( !protect.empty() )
			ofs << "#ifdef " << protect << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_leaveProtect( DualOFStream& ofs,
									  std::string const& protect ) const
	{
		if( !protect.empty() )
			ofs << "#endif /*" << protect << "*/" << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_enterProtect( std::ofstream& ofs,
									  std::string const& protect ) const
	{
		if( !protect.empty() )
			ofs << "#ifdef " << protect << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_leaveProtect( std::ofstream& ofs,
									  std::string const& protect ) const
	{
		if( !protect.empty() )
			ofs << "#endif /*" << protect << "*/" << std::endl;
	}
	//--------------------------------------------------------------------------
	std::string CppGenerator::_determineFunctionName( std::string const& name,
													  CommandData const& commandData ) const
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
				strippedName.erase( pos, commandData.arguments[ 0 ].pureType.length() );

			else if( commandData.arguments[ 0 ].pureType == "CommandBuffer"
					 && name.find( "cmd" ) == 0 )
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
	//--------------------------------------------------------------------------
	std::string CppGenerator::_determineReturnType( CommandData const& commandData,
													size_t returnIndex,
													bool isVector ) const
	{
		std::string returnType;
		if( returnIndex != ~0
			&& ( commandData.returnType == "void"
				 || ( commandData.returnType == "Result"
					  && ( commandData.successCodes.size() == 1
						   || ( commandData.successCodes.size() == 2
								&& commandData.successCodes[ 1 ] == "eIncomplete"
								&& commandData.twoStep ) ) ) ) )
		{
			if( isVector )
			{
				if( commandData.arguments[ returnIndex ].pureType == "void" )
					returnType = "std::vector<uint8_t, Allocator>";

				else
					returnType = "std::vector<" + commandData.arguments[ returnIndex ].pureType + ", Allocator>";
			}
			else
			{
				assert( commandData.arguments[ returnIndex ].type.back() == '*' );
				assert( commandData.arguments[ returnIndex ].type.find( "const" ) == std::string::npos );
				returnType = commandData.arguments[ returnIndex ].type;
				returnType.pop_back();
			}
		}
		else if( commandData.returnType == "Result" && commandData.successCodes.size() == 1 )
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
	//--------------------------------------------------------------------------
	std::string CppGenerator::_reduceName( std::string const& name ) const
	{
		std::string reducedName;
		if( name[ 0 ] == 'p' && 1 < name.length() && ( isupper( name[ 1 ] ) || name[ 1 ] == 'p' ) )
		{
			reducedName = StringsHelper::strip( name, "p" );
			reducedName[ 0 ] = tolower( reducedName[ 0 ] );
		}
		else
			reducedName = name;

		return reducedName;
	}
	//--------------------------------------------------------------------------
	size_t CppGenerator::_findReturnIndex( CommandData const& commandData,
										   std::map<size_t, size_t> const& vectorParameters ) const
	{
		if( commandData.returnType == "Result" || commandData.returnType == "void" )
		{
			for( size_t i = 0; i < commandData.arguments.size(); i++ )
			{
				if( commandData.arguments[ i ].type.find( '*' ) != std::string::npos
					&& commandData.arguments[ i ].type.find( "const" ) == std::string::npos
					&& !_isVectorSizeParameter( vectorParameters, i )
					&& ( vectorParameters.find( i ) == vectorParameters.end()
						 || commandData.twoStep
						 || commandData.successCodes.size() == 1 ) )
				{
	#if !defined(NDEBUG)
					for( size_t j = i + 1; j < commandData.arguments.size(); j++ )
					{
						assert( commandData.arguments[ j ].type.find( '*' ) == std::string::npos
								|| commandData.arguments[ j ].type.find( "const" ) != std::string::npos );
					}
	#endif
					return i;
				}
			}
		}
		return ~0;
	}
	//--------------------------------------------------------------------------
	size_t CppGenerator::_findTemplateIndex( CommandData const& commandData,
											 std::map<size_t, size_t> const& vectorParameters ) const
	{
		for( size_t i = 0; i < commandData.arguments.size(); i++ )
		{
			if( commandData.arguments[ i ].name == "pData"
				|| commandData.arguments[ i ].name == "pValues" )
			{
				assert( vectorParameters.find( i ) != vectorParameters.end() );
				return i;
			}
		}
		return ~0;
	}
	//--------------------------------------------------------------------------
	std::map<size_t, size_t> CppGenerator::_getVectorParameters( CommandData const& commandData ) const
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
						lenParameters[ i ] = j;
				}
				assert( lenParameters[ i ] != ~0
					|| commandData.arguments[ i ].len == "dataSize/4"
					|| commandData.arguments[ i ].len == "latexmath:[$dataSize \\over 4$]"
					|| commandData.arguments[ i ].len == "null-terminated"
					|| commandData.arguments[ i ].len == "pAllocateInfo->descriptorSetCount"
					|| commandData.arguments[ i ].len == "pAllocateInfo->commandBufferCount" );
				assert( lenParameters[ i ] == ~0 || lenParameters[ i ] < i );
			}
		}
		return lenParameters;
	}
	//--------------------------------------------------------------------------
	bool CppGenerator::_hasPointerArguments( CommandData const& commandData ) const
	{
		for( size_t i = 0; i < commandData.arguments.size(); i++ )
		{
			size_t pos = commandData.arguments[ i ].type.find( '*' );
			if( pos != std::string::npos &&
				commandData.arguments[ i ].type.find( '*', pos + 1 ) == std::string::npos )
				return true;
		}
		return false;
	}
	//--------------------------------------------------------------------------
	bool CppGenerator::_isVectorSizeParameter( std::map<size_t, size_t> const& vectorParameters,
											   size_t idx ) const
	{
		for( auto& it : vectorParameters )
		{
			if( it.second == idx )
				return true;
		}
		return false;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_sortDependencies( std::list<DependencyData>& dependencies ) const
	{
		std::set<std::string> listedTypes = { "VkFlags" };
		std::list<DependencyData> sortedDependencies;

		while( !dependencies.empty() )
		{
	#if !defined(NDEBUG)
			bool ok = false;
	#endif
			for( auto it = dependencies.begin(); it != dependencies.end(); ++it )
			{
				if( _noDependencies( it->dependencies, listedTypes ) )
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
	//--------------------------------------------------------------------------
	bool CppGenerator::_noDependencies( std::set<std::string> const& dependencies,
										std::set<std::string>& listedTypes ) const
	{
		for( auto& it : dependencies )
		{
			if( listedTypes.find( it ) == listedTypes.end() )
				return false;
		}

		return true;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_createDefaults( SpecData* vkData,
										std::map<std::string, std::string>& defaultValues ) const
	{
		for( auto& it : vkData->dependencies )
		{
			assert( defaultValues.find( it.name ) == defaultValues.end() );
			switch( it.category )
			{
				case DependencyData::Category::COMMAND:    // commands should never be asked for defaults
					break;

				case DependencyData::Category::ENUM:
				{
					assert( vkData->enums.find( it.name ) != vkData->enums.end() );
					auto& enumData = vkData->enums.find( it.name )->second;
					auto value = it.name;

					if( !enumData.members.empty() )
						value += "::" + vkData->enums.find( it.name )->second.members.front().name;
					else
						value += "()";

					defaultValues[ it.name ] = value;
				}
				break;

				case DependencyData::Category::FLAGS:
				case DependencyData::Category::HANDLE:
				case DependencyData::Category::STRUCT:
				case DependencyData::Category::UNION:        // just call the default constructor for flags, structs, and structs (which are mapped to classes)
					defaultValues[ it.name ] = it.name + "()";
					break;

				case DependencyData::Category::FUNC_POINTER: // func_pointers default to nullptr
					defaultValues[ it.name ] = "nullptr";
					break;

				case DependencyData::Category::REQUIRED:     // all required default to "0"
				case DependencyData::Category::SCALAR:       // all scalars default to "0"
					defaultValues[ it.name ] = "0";
					break;

				default:
					assert( false );
					break;
			}
		}
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeVersionCheck( std::ofstream& ofs,
										  std::string const& version ) const
	{
		ofs << "static_assert( VK_HEADER_VERSION == " << version
			<< " , \"Wrong VK_HEADER_VERSION!\" );\n\n";
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypesafeCheck( std::ofstream& ofs,
											std::string const& typesafeCheck ) const
	{
		ofs << "// 32-bit vulkan is not typesafe for handles, so don't allow copy constructors on this platform by default.\n"
			<< "// To enable this feature on 32-bit platforms please define VK_CPP_TYPESAFE_CONVERSION\n"
			<< typesafeCheck << std::endl
			<< "#define VK_CPP_TYPESAFE_CONVERSION 1\n"
			<< "#endif\n\n";
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeEnumsToString( DualOFStream& ofs, SpecData* vkData ) const
	{
		for( auto& it : vkData->dependencies )
		{
			switch( it.category )
			{
				case DependencyData::Category::ENUM:
				{
					assert( vkData->enums.find( it.name ) != vkData->enums.end() );
					_writeEnumsToString(
								ofs,
								it,
								vkData->enums.find( it.name )->second
					);
				}
				break;

				case DependencyData::Category::FLAGS:
					_writeFlagsToString(
								ofs,
								it,
								vkData->enums.find( *it.dependencies.begin() )->second
					);
				break;
			}
		}
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeEnumsToString( DualOFStream& ofs,
											DependencyData const& dependencyData,
											EnumData const& enumData ) const
	{
		_enterProtect( ofs, enumData.protect );
		ofs << "  ";
		if( !ofs.usingDualStream() )
			ofs << "inline ";

		ofs << "std::string to_string( " << dependencyData.name
			<< ( enumData.members.empty() ? " )" : " value )" );
		if( ofs.usingDualStream() )
			ofs.hdr() << ";";

		ofs << std::endl;
		ofs.src() << "  {\n";

		if( enumData.members.empty() )
			ofs.src() << "    return \"(void)\";\n";
		else
		{
			ofs.src() << "    switch( value )\n    {\n";
			for( auto& itMember : enumData.members )
			{
				ofs.src() << "    case " << dependencyData.name << "::"
					<< itMember.name << ": return \""
					<< itMember.name.substr( 1 ) << "\";\n";
			}
			ofs.src() << "    default: return \"invalid\";\n"
				<< "    }\n";
		}
		ofs.src() << "  }\n";
		_leaveProtect( ofs, enumData.protect );
		ofs << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeFlagsToString( DualOFStream& ofs,
											DependencyData const& dependencyData,
											EnumData const &enumData ) const
	{
		_enterProtect( ofs, enumData.protect );
		ofs << "  ";
		if( !ofs.usingDualStream() )
			ofs << "inline ";

		ofs << "std::string to_string( const " << dependencyData.name
			<< ( enumData.members.empty() ? "& )" : "& value )" );

		if( ofs.usingDualStream() )
			ofs.hdr() << ";";

		ofs << std::endl;
		ofs.src() << "  {\n";

		if( enumData.members.empty() )
			ofs.src() << "    return \"{}\";\n";
		else
		{
			std::string enumPrefix = *dependencyData.dependencies.begin() + "::";

			ofs.src() << "    if( !value ) return \"{}\";\n"
					  << "    std::string result;\n";

			for( auto& itMember : enumData.members )
			{
				ofs.src() << "    if( value & " << enumPrefix + itMember.name
						  << " ) result += \"" << itMember.name.substr( 1 )
						  << " | \";\n";
			}

			ofs.src() << "    return \"{\" + result.substr( 0, result.size() - 3 ) + \"}\";\n";
		}
		ofs.src() << "  }\n";
		_leaveProtect( ofs, enumData.protect );
		ofs << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypes( DualOFStream& ofs, SpecData* vkData,
									std::map<std::string, std::string> const& defaultValues )
	{
		for( auto& it : vkData->dependencies )
		{
			switch( it.category )
			{
				case DependencyData::Category::COMMAND:
					_writeTypeCommand( ofs.hdr(), vkData, it );
					break;

				case DependencyData::Category::ENUM:
					assert( vkData->enums.find( it.name ) != vkData->enums.end() );
					_writeTypeEnum( ofs.hdr(), it, vkData->enums.find( it.name )->second );
					break;

				case DependencyData::Category::FLAGS:
					assert( vkData->flags.find( it.name ) != vkData->flags.end() );
					_writeTypeFlags( ofs, it, vkData->flags.find( it.name )->second );
					break;

				case DependencyData::Category::FUNC_POINTER:
				case DependencyData::Category::REQUIRED:
					// skip FUNC_POINTER and REQUIRED, they just needed to be in the dependencies list to resolve dependencies
					break;

				case DependencyData::Category::HANDLE:
					assert( vkData->handles.find( it.name ) != vkData->handles.end() );
					_writeTypeHandle( ofs, vkData, it, vkData->handles.find( it.name )->second, vkData->dependencies );
					break;

				case DependencyData::Category::SCALAR:
					_writeTypeScalar( ofs.hdr(), it );
					break;

				case DependencyData::Category::STRUCT:
					_writeTypeStruct( ofs.hdr(), vkData, it, defaultValues );
					break;

				case DependencyData::Category::UNION:
					assert( vkData->structs.find( it.name ) != vkData->structs.end() );
					_writeTypeUnion( ofs.hdr(), vkData, it, vkData->structs.find( it.name )->second, defaultValues );
					break;

				default:
					assert( false );
					break;
			}
		}
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeCommand( std::ofstream& ofs, SpecData* vkData,
										  DependencyData const& dependencyData ) const
	{
		auto it = vkData->commands.find( dependencyData.name );
		assert( it != vkData->commands.end() );
		auto& commandData = it->second;
		if( !commandData.handleCommand )
		{
			_writeTypeCommandStandard( ofs, "  ", dependencyData.name, dependencyData, commandData, vkData->vkTypes );

			ofs << "\n#ifndef VKCPP_DISABLE_ENHANCED_MODE\n";
			_writeTypeCommandEnhanced( ofs, vkData, "  ", "", dependencyData.name, dependencyData, commandData );
			ofs << "#endif /*VKCPP_DISABLE_ENHANCED_MODE*/\n\n";
		}
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeCommandStandard( std::ofstream& ofs,
												  std::string const& indentation,
												  std::string const& functionName,
												  DependencyData const& dependencyData,
												  CommandData const& commandData,
												  std::set<std::string> const& vkTypes ) const
	{
		_enterProtect( ofs, commandData.protect );
		ofs << indentation;
		if( !commandData.handleCommand )
			ofs << "inline ";

		ofs << commandData.returnType << " " << functionName << "( ";
		bool argEncountered = false;
		for( size_t i = commandData.handleCommand ? 1 : 0; i < commandData.arguments.size(); i++ )
		{
			if( argEncountered )
				ofs << ", ";

			ofs << commandData.arguments[ i ].type << " " << commandData.arguments[ i ].name;
			if( !commandData.arguments[ i ].arraySize.empty() )
				ofs << "[ " << commandData.arguments[ i ].arraySize << " ]";

			argEncountered = true;
		}
		ofs << " )";
		if( commandData.handleCommand )
			ofs << " const";

		ofs << std::endl << indentation << "{\n" << indentation << "  ";

		bool castReturn = false;
		if( commandData.returnType != "void" )
		{
			ofs << "return ";
			castReturn = ( vkTypes.find( commandData.returnType ) != vkTypes.end() );
			if( castReturn )
				ofs << "static_cast<" << commandData.returnType << ">( ";
		}

		std::string callName( dependencyData.name );
		assert( islower( callName[ 0 ] ) );
		callName[ 0 ] = toupper( callName[ 0 ] );

		ofs << "vk" << callName << "( ";
		if( commandData.handleCommand )
			ofs << "m_" << commandData.arguments[ 0 ].name;

		argEncountered = false;
		for( size_t i = commandData.handleCommand ? 1 : 0; i < commandData.arguments.size(); i++ )
		{
			if( 0 < i )
				ofs << ", ";

			_writeMemberData( ofs, commandData.arguments[ i ], vkTypes );
		}
		ofs << " )";
		if( castReturn )
			ofs << " )";

		ofs << ";\n" << indentation << "}\n";
		_leaveProtect( ofs, commandData.protect );
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeCommandEnhanced( std::ofstream& ofs, SpecData* vkData,
												  std::string const& indentation,
												  std::string const& className,
												  std::string const& functionName,
												  DependencyData const& dependencyData,
												  CommandData const& commandData ) const
	{
		_enterProtect( ofs, commandData.protect );
		std::map<size_t, size_t> vectorParameters = _getVectorParameters( commandData );
		size_t returnIndex = _findReturnIndex( commandData, vectorParameters );
		size_t templateIndex = _findTemplateIndex( commandData, vectorParameters );
		auto returnVector = vectorParameters.find( returnIndex );
		std::string returnType = _determineReturnType( commandData, returnIndex, returnVector != vectorParameters.end() );

		_writeFunctionHeader( ofs, vkData, indentation, returnType, functionName, commandData, returnIndex, templateIndex, vectorParameters );
		_writeFunctionBody( ofs, indentation, className, functionName, returnType, templateIndex, dependencyData, commandData, vkData->vkTypes, returnIndex, vectorParameters );
		_leaveProtect( ofs, commandData.protect );
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeEnum( std::ofstream& ofs,
									   DependencyData const& dependencyData,
									   EnumData const& enumData ) const
	{
		_enterProtect( ofs, enumData.protect );
		ofs << "  enum class " << dependencyData.name;
		if( enumData.members.empty() )
			ofs << " {};\n";
		else
		{
			ofs << "\n  {\n";
			for( size_t i = 0; i < enumData.members.size(); i++ )
			{
				ofs << "    " << enumData.members[ i ].name << " = "
					<< enumData.members[ i ].value << std::endl;
			}
			ofs << "  };\n";
		}
		_leaveProtect( ofs, enumData.protect );
		ofs << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeFlags( DualOFStream& ofs,
										DependencyData const& dependencyData,
										FlagData const& flagData )
	{
		assert( dependencyData.dependencies.size() == 1 );
		_enterProtect( ofs, flagData.protect );
		auto& firstDep = *dependencyData.dependencies.begin();
		auto& depName = dependencyData.name;

		ofs.hdr() << ++_indent
				  << "using " << depName << " = Flags<" << firstDep
				  << ", Vk" << depName << ">;\n\n";

		ofs << _indent;
		if( !ofs.usingDualStream() )
			ofs << "inline ";

		ofs << depName << " operator|( " << firstDep << " bit0, " << firstDep << " bit1 )";
		if( ofs.usingDualStream() )
			ofs.hdr() << ";";

		ofs << std::endl;
		ofs.src() << _indent << "{\n";
		ofs.src() << _indent + 1 << "return " << depName << "( bit0 ) | bit1;\n";
		ofs.src() << _indent << "}\n";

		--_indent;
		_leaveProtect( ofs, flagData.protect );
		ofs << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeHandle( DualOFStream& ofs, SpecData* vkData,
										 DependencyData const& dependencyData,
										 HandleData const& handle,
										 std::list<DependencyData> const& dependencies )
	{
		std::string memberName = dependencyData.name;
		assert( isupper( memberName[ 0 ] ) );
		memberName[ 0 ] = tolower( memberName[ 0 ] );

		ofs.hdr() << ++_indent << "class " << dependencyData.name
				  << "\n" << _indent << "{\n" << _indent << "public:\n";

		ofs << ++_indent << dependencyData.name;

		if( ofs.usingDualStream() )
		{
			ofs.hdr() << "();";
			ofs.src() << "::" << dependencyData.name << "()";
		}
		else
			ofs.hdr() << "()";

		ofs << std::endl;
		ofs.src() << ++_indent << ": m_" << memberName << "( VK_NULL_HANDLE )\n";
		ofs.src() << --_indent << "{}\n\n";

		ofs << "#ifdef VK_CPP_TYPESAFE_CONVERSION\n" << _indent;
		// construct from native handle
		if( ofs.usingDualStream() )
			ofs.src() << dependencyData.name << "::";

		ofs << dependencyData.name << "( Vk" << dependencyData.name << " " << memberName << " )";

		if( ofs.usingDualStream() )
			ofs.hdr() << ";";

		ofs << std::endl;
		ofs.src() << ++_indent << ": m_" << memberName << "( " << memberName << " )\n";
		ofs.src() << --_indent << "{}\n\n";

		// assignment from native handle
		ofs << _indent << dependencyData.name << "& ";
		if( ofs.usingDualStream() )
			ofs.src() << dependencyData.name << "::";

		ofs << "operator=( Vk" << dependencyData.name << " " << memberName << " )";

		if( ofs.usingDualStream() )
			ofs.hdr() << ";\n";

		ofs << std::endl;
		ofs.src() << _indent << "{\n";
		ofs.src() << ++_indent << "m_" << memberName << " = " << memberName << ";\n"
				  << _indent << "return *this;\n";
		ofs.src() << --_indent << "}\n";
		ofs << "#endif\n\n";

		ofs << "#ifndef VK_CPP_TYPESAFE_CONVERSION\n";
		ofs.hdr() << _indent << "explicit\n";
		ofs << ( _indent -= 2 ) << "#endif\n";

		ofs << ( _indent += 2 );
		if( ofs.usingDualStream() )
			ofs.src() << dependencyData.name << "::";

		ofs << "operator Vk" << dependencyData.name << "() const";
		if( ofs.usingDualStream() )
			ofs.hdr() << ";";

		ofs << std::endl;
		ofs.src() << _indent << "{\n";
		ofs.src() << ++_indent << "return m_" << memberName << ";\n";
		ofs.src() << --_indent << "}\n\n";

		if( !handle.commands.empty() )
		{
			for( size_t i = 0; i < handle.commands.size(); i++ )
			{
				std::string commandName = handle.commands[ i ];
				auto cit = vkData->commands.find( commandName );
				assert( cit != vkData->commands.end() && cit->second.handleCommand );
				auto dep = std::find_if(
							dependencies.begin(),
							dependencies.end(),
							[ commandName ]( DependencyData const& dd ) { return dd.name == commandName; }
				);
				assert( dep != dependencies.end() );
				std::string className = dependencyData.name;
				std::string functionName = _determineFunctionName( dep->name, cit->second );

				bool hasPointers = _hasPointerArguments( cit->second );
				if( !hasPointers )
					ofs << "#ifdef VKCPP_DISABLE_ENHANCED_MODE\n";

				_writeTypeCommandStandard( ofs.hdr(), "    ", functionName, *dep, cit->second, vkData->vkTypes );
				if( !hasPointers )
					ofs << "#endif /*!VKCPP_DISABLE_ENHANCED_MODE*/\n";

				ofs << "\n#ifndef VKCPP_DISABLE_ENHANCED_MODE\n";
				_writeTypeCommandEnhanced( ofs.hdr(), vkData, "    ", className, functionName, *dep, cit->second );
				ofs << "#endif /*VKCPP_DISABLE_ENHANCED_MODE*/\n";

				if( i < handle.commands.size() - 1 )
					ofs << std::endl;
			}
			ofs << std::endl;
		}

		ofs.hdr() << _indent << "explicit ";
		if( ofs.usingDualStream() )
			ofs.src() << _indent << dependencyData.name << "::";

		ofs << "operator bool() const";
		if( ofs.usingDualStream() )
			ofs.hdr() << ";";

		ofs << std::endl;
		ofs.src() << _indent << "{\n";
		ofs.src() << ++_indent << "return m_" << memberName << " != VK_NULL_HANDLE;\n";
		ofs.src() << --_indent << "}\n\n";

		ofs << _indent << "bool ";
		if( ofs.usingDualStream() )
			ofs.src() << dependencyData.name << "::";

		ofs << "operator!() const";
		if( ofs.usingDualStream() )
			ofs.hdr() << ";";

		ofs << std::endl;
		ofs.src() << _indent  << "{\n";
		ofs.src() << ++_indent << "return m_" << memberName << " == VK_NULL_HANDLE;\n";
		ofs.src() << --_indent << "}\n\n";

		ofs.hdr() << --_indent << "private:\n";
		ofs.hdr() << _indent + 1 << "Vk" << dependencyData.name << " m_" << memberName << ";\n";
		ofs.hdr() << _indent << "};\n";
	#if 1
		ofs.src() << _indent << "static_assert( sizeof( " << dependencyData.name
				  << " ) == sizeof( Vk" << dependencyData.name
				  << " ), \"handle and wrapper have different size!\" );\n";
	#endif
		--_indent;
		ofs << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeScalar( std::ofstream& ofs,
										 DependencyData const& dependencyData ) const
	{
		assert( dependencyData.dependencies.size() == 1 );
		ofs << "  using " << dependencyData.name << " = "
			<< *dependencyData.dependencies.begin() << ";\n\n";
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeStruct( std::ofstream& ofs, SpecData* vkData,
										 DependencyData const& dependencyData,
										 std::map<std::string, std::string> const& defaultValues ) const
	{
		auto it = vkData->structs.find( dependencyData.name );
		assert( it != vkData->structs.end() );

		_enterProtect( ofs, it->second.protect );
		ofs << "  struct " << dependencyData.name << std::endl
			<< "  {\n";

		// only structs that are not returnedOnly get a constructor!
		if( !it->second.returnedOnly )
			_writeStructConstructor( ofs, dependencyData.name, it->second, vkData->vkTypes, defaultValues );

		// create the setters
		if( !it->second.returnedOnly )
		{
			for( size_t i = 0; i < it->second.members.size(); i++ )
				_writeStructSetter( ofs, dependencyData.name, it->second.members[ i ], vkData->vkTypes );
		}

		// the cast-operator to the wrapped struct
		ofs << "    operator const Vk" << dependencyData.name << "&() const\n"
			<< "    {\n"
			<< "      return *reinterpret_cast<const Vk" << dependencyData.name << "*>( this );\n"
			<< "    }\n\n";

		// the member variables
		for( size_t i = 0; i < it->second.members.size(); i++ )
		{
			if( it->second.members[ i ].type == "StructureType" )
			{
				assert( i == 0 && it->second.members[ i ].name == "sType" );
				ofs << "  private:\n"
					<< "    StructureType sType;\n\n"
					<< "  public:\n";
			}
			else
			{
				ofs << "    " << it->second.members[ i ].type << " " << it->second.members[ i ].name;
				if( !it->second.members[ i ].arraySize.empty() )
					ofs << "[ " << it->second.members[ i ].arraySize << " ]";
				ofs << ";\n";
			}
		}
		ofs << "  };\n";
	#if 1
		ofs << "  static_assert( sizeof( " << dependencyData.name << " ) == sizeof( Vk" << dependencyData.name << " ), \"struct and wrapper have different size!\" );" << std::endl;
	#endif
		_leaveProtect( ofs, it->second.protect );
		ofs << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeTypeUnion( std::ofstream& ofs, SpecData* vkData,
										DependencyData const& dependencyData,
										StructData const& unionData,
										std::map<std::string, std::string> const& defaultValues ) const
	{
		//Unused variable
		//std::ostringstream oss;
		ofs << "  union " << dependencyData.name << std::endl
			<< "  {\n";

		for( size_t i = 0; i < unionData.members.size(); i++ )
		{
			// one constructor per union element
			ofs << "    " << dependencyData.name << "( ";
			if( unionData.members[ i ].arraySize.empty() )
				ofs << unionData.members[ i ].type << " ";

			else
				ofs << "const std::array<" << unionData.members[ i ].type << ", " << unionData.members[ i ].arraySize << ">& ";

			ofs << unionData.members[ i ].name << "_";

			// just the very first constructor gets default arguments
			if( i == 0 )
			{
				auto it = defaultValues.find( unionData.members[ i ].pureType );
				assert( it != defaultValues.end() );
				if( unionData.members[ i ].arraySize.empty() )
					ofs << " = " << it->second;
				else
					ofs << " = { " << it->second << " }";
			}
			ofs << " )\n    {\n      ";

			if( unionData.members[ i ].arraySize.empty() )
				ofs << unionData.members[ i ].name << " = " << unionData.members[ i ].name << "_";
			else
			{
				ofs << "memcpy( &" << unionData.members[ i ].name << ", "
					<< unionData.members[ i ].name << "_.data(), "
					<< unionData.members[ i ].arraySize
					<< " * sizeof( " << unionData.members[ i ].type << " ) )";
			}
			ofs << ";\n    }\n\n";
		}

		for( size_t i = 0; i < unionData.members.size(); i++ )
		{
			// one setter per union element
			assert( !unionData.returnedOnly );
			_writeStructSetter( ofs, dependencyData.name, unionData.members[ i ], vkData->vkTypes );
		}

		// the implicit cast operator to the native type
		ofs << "    operator Vk" << dependencyData.name << " const& () const\n"
			<< "    {\n"
			<< "      return *reinterpret_cast<const Vk" << dependencyData.name << "*>( this );\n"
			<< "    }\n\n";

		// the union member variables
		// if there's at least one Vk... type in this union, check for unrestricted unions support
		bool needsUnrestrictedUnions = false;
		for( size_t i = 0; i < unionData.members.size() && !needsUnrestrictedUnions; i++ )
			needsUnrestrictedUnions = vkData->vkTypes.find( unionData.members[ i ].type ) != vkData->vkTypes.end();

		if( needsUnrestrictedUnions )
		{
			ofs << "#ifdef VK_CPP_HAS_UNRESTRICTED_UNIONS\n";
			for( size_t i = 0; i < unionData.members.size(); i++ )
			{
				ofs << "    " << unionData.members[ i ].type << " " << unionData.members[ i ].name;
				if( !unionData.members[ i ].arraySize.empty() )
					ofs << "[ " << unionData.members[ i ].arraySize << " ]";

				ofs << ";\n";
			}
			ofs << "#else\n";
		}
		for( size_t i = 0; i < unionData.members.size(); i++ )
		{
			ofs << "    ";
			if( vkData->vkTypes.find( unionData.members[ i ].type ) != vkData->vkTypes.end() )
				ofs << "Vk";

			ofs << unionData.members[ i ].type << " " << unionData.members[ i ].name;
			if( !unionData.members[ i ].arraySize.empty() )
				ofs << "[ " << unionData.members[ i ].arraySize << " ]";

			ofs << ";\n";
		}
		if( needsUnrestrictedUnions )
			ofs << "#endif  // VK_CPP_HAS_UNRESTRICTED_UNIONS\n";

		ofs << "  };\n\n";
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeStructConstructor( std::ofstream& ofs,
												std::string const& name,
												StructData const& structData,
												std::set<std::string> const& /*vkTypes*/, //Unused variable
												std::map<std::string,
												std::string> const& defaultValues ) const
	{
		// the constructor with all the elements as arguments, with defaults
		ofs << "    " << name << "( ";
		bool listedArgument = false;
		for( size_t i = 0; i < structData.members.size(); i++ )
		{
			if( listedArgument )
				ofs << ", ";

			if( structData.members[ i ].name != "pNext" && structData.members[ i ].name != "sType" )
			{
				auto defaultIt = defaultValues.find( structData.members[ i ].pureType );
				assert( defaultIt != defaultValues.end() );
				if( structData.members[ i ].arraySize.empty() )
				{
					ofs << structData.members[ i ].type << " "
						<< structData.members[ i ].name
						<< "_ = " << ( structData.members[ i ].type.back() == '*' ? "nullptr" : defaultIt->second );
				}
				else
				{
					ofs << "std::array<" << structData.members[ i ].type << ", "
						<< structData.members[ i ].arraySize << "> const& "
						<< structData.members[ i ].name << "_ = { "
						<< defaultIt->second;

					size_t n = atoi( structData.members[ i ].arraySize.c_str() );
					assert( 0 < n );
					for( size_t j = 1; j < n; j++ )
						ofs << ", " << defaultIt->second;

					ofs << " }";
				}
				listedArgument = true;
			}
		}
		ofs << " )\n";

		// copy over the simple arguments
		bool firstArgument = true;
		for( size_t i = 0; i < structData.members.size(); i++ )
		{
			if( structData.members[ i ].arraySize.empty() )
			{
				ofs << "      " << ( firstArgument ? ":" : "," ) << " " << structData.members[ i ].name << "( ";
				if( structData.members[ i ].name == "pNext" )
					ofs << "nullptr";

				else if( structData.members[ i ].name == "sType" )
					ofs << "StructureType::e" << name;

				else
					ofs << structData.members[ i ].name << "_";

				ofs << " )\n";
				firstArgument = false;
			}
		}

		// the body of the constructor, copying over data from argument list into wrapped struct
		ofs << "    {\n";
		for( size_t i = 0; i < structData.members.size(); i++ )
		{
			if( !structData.members[ i ].arraySize.empty() )
			{
				ofs << "      memcpy( &" << structData.members[ i ].name
					<< ", " << structData.members[ i ].name << "_.data(), "
					<< structData.members[ i ].arraySize
					<< " * sizeof( " << structData.members[ i ].type << " ) );\n";
			}
		}
		ofs << "    }\n\n";

		// the copy constructor from a native struct (Vk...)
		ofs << "    " << name << "( Vk" << name << " const & rhs )\n"
			<< "    {\n"
			<< "      memcpy( this, &rhs, sizeof( " << name << " ) );\n"
			<< "    }\n\n";

		// the assignment operator from a native sturct (Vk...)
		ofs << "    " << name << "& operator=( Vk" << name << " const & rhs )\n"
			<< "    {\n"
			<< "      memcpy( this, &rhs, sizeof( " << name << " ) );\n"
			<< "      return *this;\n"
			<< "    }\n\n";
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeStructSetter( std::ofstream& ofs,
										   std::string const& name,
										   MemberData const& memberData,
										   std::set<std::string> const& /*vkTypes*/ ) const //Unused variable
	{
		ofs << "    " << name << "& set" << static_cast<char>( toupper( memberData.name[ 0 ] ) ) << memberData.name.substr( 1 ) << "( ";
		if( memberData.arraySize.empty() )
			ofs << memberData.type << " ";

		else
			ofs << "std::array<" << memberData.type << ", " << memberData.arraySize << "> ";

		ofs << memberData.name << "_ )\n    {\n";

		if( !memberData.arraySize.empty() )
		{
			ofs << "      memcpy( &" << memberData.name << ", " << memberData.name
				<< "_.data(), " << memberData.arraySize << " * sizeof( " << memberData.type << " ) )";
		}
		else
			ofs << "      " << memberData.name << " = " << memberData.name << "_";

		ofs << ";\n      return *this;\n" << "    }\n\n";
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeMemberData( std::ofstream& ofs,
										 MemberData const& memberData,
										 std::set<std::string> const& vkTypes ) const
	{
		if( vkTypes.find( memberData.pureType ) != vkTypes.end() )
		{
			if( memberData.type.back() == '*' )
			{
				ofs << "reinterpret_cast<";
				if( memberData.type.find( "const" ) == 0 )
					ofs << "const ";

				ofs << "Vk" << memberData.pureType << '*';
			}
			else
				ofs << "static_cast<Vk" << memberData.pureType;

			ofs << ">( " << memberData.name << " )";
		}
		else
			ofs << memberData.name;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeFunctionHeader( std::ofstream& ofs, SpecData* vkData,
											 std::string const& indentation,
											 std::string const& returnType,
											 std::string const& name,
											 CommandData const& commandData,
											 size_t returnIndex,
											 size_t templateIndex,
											 std::map<size_t, size_t> const& vectorParameters ) const
	{
		std::set<size_t> skippedArguments;
		for( auto& it : vectorParameters )
		{
			if( it.second != ~0 )
				skippedArguments.insert( it.second );
		}
		if( vectorParameters.size() == 1
			&& ( commandData.arguments[ vectorParameters.begin()->first ].len == "dataSize/4"
				 || commandData.arguments[ vectorParameters.begin()->first ].len == "latexmath:[$dataSize \\over 4$]" ) )
		{
			assert( commandData.arguments[ 3 ].name == "dataSize" );
			skippedArguments.insert( 3 );
		}
		if( returnIndex != ~0 )
			skippedArguments.insert( returnIndex );

		ofs << indentation;
		if( templateIndex != ~0 && ( templateIndex != returnIndex || returnType == "Result" ) )
		{
			assert( returnType.find( "Allocator" ) == std::string::npos );
			ofs << "template <typename T>\n" << indentation;
		}
		else if( returnType.find( "Allocator" ) != std::string::npos )
		{
			assert( returnType.substr( 0, 12 ) == "std::vector<" && returnType.find( ',' ) != std::string::npos && 12 < returnType.find( ',' ) );
			ofs << "template <typename Allocator = std::allocator<"
				<< returnType.substr( 12, returnType.find( ',' ) - 12 ) << ">>\n"
				<< indentation;

			if( returnType != commandData.returnType && commandData.returnType != "void" )
				ofs << "typename ";
		}
		else if( !commandData.handleCommand )
			ofs << "inline ";

		if( returnType != commandData.returnType && commandData.returnType != "void" )
		{
			assert( commandData.returnType == "Result" );
			ofs << "ResultValueType<" << returnType << ">::type ";
		}
		else if( returnIndex != ~0 && 1 < commandData.successCodes.size() )
		{
			assert( commandData.returnType == "Result" );
			ofs << "ResultValue<" << commandData.arguments[ returnIndex ].pureType << "> ";
		}
		else
			ofs << returnType << " ";

		ofs << _reduceName( name ) << "(";
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
						ofs << ", ";

					auto it = vectorParameters.find( i );
					size_t pos = commandData.arguments[ i ].type.find( '*' );
					if( it == vectorParameters.end() )
					{
						if( pos == std::string::npos )
						{
							ofs << commandData.arguments[ i ].type << " " << _reduceName( commandData.arguments[ i ].name );
							if( !commandData.arguments[ i ].arraySize.empty() )
								ofs << "[ " << commandData.arguments[ i ].arraySize << " ]";

							if( lastArgument == i )
							{
								auto flagIt = vkData->flags.find( commandData.arguments[ i ].pureType );
								if( flagIt != vkData->flags.end() )
								{
									auto depIt = std::find_if( vkData->dependencies.begin(), vkData->dependencies.end(), [ &flagIt ]( DependencyData const& dd ) { return( dd.name == flagIt->first ); } );
									assert( depIt != vkData->dependencies.end() );
									assert( depIt->dependencies.size() == 1 );
									auto enumIt = vkData->enums.find( *depIt->dependencies.begin() );
									assert( enumIt != vkData->enums.end() );
									if( enumIt->second.members.empty() )
										ofs << " = " << commandData.arguments[ i ].pureType << "()";
								}
							}
						}
						else
						{
							assert( commandData.arguments[ i ].type[ pos ] == '*' );
							auto n = _reduceName( commandData.arguments[ i ].name );

							if( commandData.arguments[ i ].optional )
							{
								ofs << "Optional<"
									<< StringsHelper::trimEnd( commandData.arguments[ i ].type.substr( 0, pos ) )
									<< "> " << n << " = nullptr";
							}
							else if( commandData.arguments[ i ].type.find( "char" ) == std::string::npos )
							{
								ofs << StringsHelper::trimEnd( commandData.arguments[ i ].type.substr( 0, pos ) )
									<< "& " << n;
							}
							else
								ofs << "const std::string& " << n;
						}
					}
					else
					{
						bool optional = commandData.arguments[ i ].optional && ( it == vectorParameters.end() || it->second == ~0 );
						assert( pos != std::string::npos );
						assert( commandData.arguments[ i ].type[ pos ] == '*' );
						auto n = _reduceName( commandData.arguments[ i ].name );
						if( commandData.arguments[ i ].type.find( "char" ) != std::string::npos )
						{
							if( optional )
								ofs << "Optional<const std::string> " << n << " = nullptr";
							else
								ofs << "const std::string& " << n;
						}
						else
						{
							assert( !optional );
							bool isConst = ( commandData.arguments[ i ].type.find( "const" ) != std::string::npos );
							ofs << "ArrayProxy<" << ( templateIndex == i ? ( isConst ? "const T" : "T" ) : StringsHelper::trimEnd( commandData.arguments[ i ].type.substr( 0, pos ) ) ) << "> " << n;
						}
					}
					argEncountered = true;
				}
			}
			ofs << " ";
		}
		ofs << ")";
		if( commandData.handleCommand )
			ofs << " const";

		ofs << std::endl;
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeFunctionBody( std::ofstream& ofs,
										   std::string const& indentation,
										   std::string const& className,
										   std::string const& functionName,
										   std::string const& returnType,
										   size_t templateIndex,
										   DependencyData const& dependencyData,
										   CommandData const& commandData,
										   std::set<std::string> const& vkTypes,
										   size_t returnIndex,
										   std::map<size_t, size_t> const& vectorParameters ) const
	{
		ofs << indentation << "{\n";

		// add a static_assert if a type is templated and its size needs to be some multiple of the original size
		if( templateIndex != ~0 && commandData.arguments[ templateIndex ].pureType != "void" )
		{
			ofs << indentation << "  static_assert( sizeof( T ) % sizeof( "
				<< commandData.arguments[ templateIndex ].pureType
				<< " ) == 0, \"wrong size of template type T\" );\n";
		}

		// add some error checks if multiple vectors need to have the same size
		if( 1 < vectorParameters.size() )
		{
			for( auto it0 = vectorParameters.begin(); it0 != vectorParameters.end(); ++it0 )
			{
				if( it0->first == returnIndex )
					continue;

				for( auto it1 = std::next( it0 ); it1 != vectorParameters.end(); ++it1 )
				{
					if( it1->first == returnIndex || it0->second != it1->second )
						continue;

					auto n0 = _reduceName( commandData.arguments[ it0->first ].name );
					auto n1 = _reduceName( commandData.arguments[ it1->first ].name );

					ofs << "#ifdef VK_CPP_NO_EXCEPTIONS\n"
						<< indentation << "  assert( " << n0 << ".size() == " << n1 << ".size() );\n"
						<< "#else\n"
						<< indentation << "  if ( " << n0 << ".size() != " << n1 << ".size() )\n"
						<< indentation << "  {\n"
						<< indentation << "    throw std::logic_error( \"vk::"
						<< className << "::" << functionName << ": " << n0 << ".size() != " << n1 << ".size()\" );\n"
						<< indentation << "  }\n"
						<< "#endif  // VK_CPP_NO_EXCEPTIONS\n";
				}
			}
		}

		// write the local variable to hold a returned value
		if( returnIndex != ~0 )
		{
			if( commandData.returnType != returnType )
			{
				ofs << indentation << "  " << returnType << " "
					<< _reduceName( commandData.arguments[ returnIndex ].name );

				auto it = vectorParameters.find( returnIndex );
				if( it != vectorParameters.end() && !commandData.twoStep )
				{
					std::string size;
					if( it->second == ~0 && !commandData.arguments[ returnIndex ].len.empty() )
					{
						size = _reduceName( commandData.arguments[ returnIndex ].len );
						size_t pos = size.find( "->" );
						assert( pos != std::string::npos );
						size.replace( pos, 2, "." );
					}
					else
					{
						for( auto& sit : vectorParameters )
						{
							if( sit.first != returnIndex && sit.second == it->second )
							{
								size = _reduceName( commandData.arguments[ sit.first ].name ) + ".size()";
								break;
							}
						}
					}
					assert( !size.empty() );
					ofs << "( " << size << " )";
				}
				ofs << ";\n";
			}
			else if( 1 < commandData.successCodes.size() )
			{
				ofs << indentation << "  "
					<< commandData.arguments[ returnIndex ].pureType
					<< " " << _reduceName( commandData.arguments[ returnIndex ].name )
					<< ";\n";
			}
		}

		// local count variable to hold the size of the vector to fill
		if( commandData.twoStep )
		{
			assert( returnIndex != ~0 );

			auto returnit = vectorParameters.find( returnIndex );
			assert( returnit != vectorParameters.end() && returnit->second != ~0 );
			assert( commandData.returnType == "Result" || commandData.returnType == "void" );

			ofs << indentation << "  "
				<< commandData.arguments[ returnit->second ].pureType << " "
				<< _reduceName( commandData.arguments[ returnit->second ].name )
					<< ";\n";
		}

		// write the function call
		ofs << indentation << "  ";
		std::string localIndentation = "  ";
		if( commandData.returnType == "Result" )
		{
			ofs << "Result result";
			if( commandData.twoStep && ( 1 < commandData.successCodes.size() ) )
			{
				ofs << ";\n"
					<< indentation << "  do\n"
					<< indentation << "  {\n"
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
		_writeCall( ofs, dependencyData.name, templateIndex, commandData, vkTypes, vectorParameters, returnIndex, true );
		if( commandData.returnType == "Result" )
			ofs << " )";

		ofs << ";\n";

		if( commandData.twoStep )
		{
			auto returnit = vectorParameters.find( returnIndex );

			if( commandData.returnType == "Result" )
			{
				ofs << indentation << localIndentation << "if( result == Result::eSuccess && "
					<< _reduceName( commandData.arguments[ returnit->second ].name ) << " )\n"
					<< indentation << localIndentation << "{\n"
					<< indentation << localIndentation << "  ";
			}
			else
				ofs << indentation << "  ";

			// resize the vector to hold the data according to the result from the first call
			ofs << _reduceName( commandData.arguments[ returnit->first ].name )
					<< ".resize( " << _reduceName( commandData.arguments[ returnit->second ].name ) << " );\n";

			// write the function call a second time
			if( commandData.returnType == "Result" )
				ofs << indentation << localIndentation << "  result = static_cast<Result>( ";

			else
				ofs << indentation << "  ";

			_writeCall( ofs, dependencyData.name, templateIndex, commandData, vkTypes, vectorParameters, returnIndex, false );
			if( commandData.returnType == "Result" )
				ofs << " )";

			ofs << ";\n";
			if( commandData.returnType == "Result" )
			{
				ofs << indentation << localIndentation << "}\n";
				if( 1 < commandData.successCodes.size() )
					ofs << indentation << "  } while( result == Result::eIncomplete );\n";
			}
		}

		if( commandData.returnType == "Result" || !commandData.successCodes.empty() )
		{
			ofs << indentation << "  return createResultValue( result, ";
			if( returnIndex != ~0 )
				ofs << _reduceName( commandData.arguments[ returnIndex ].name ) << ", ";

			ofs << "\"vk::" << ( className.empty() ? "" : className + "::" ) << functionName << "\"";
			if( 1 < commandData.successCodes.size() && !commandData.twoStep )
			{
				ofs << ", { Result::" << commandData.successCodes[ 0 ];
				for( size_t i = 1; i < commandData.successCodes.size(); i++ )
					ofs << ", Result::" << commandData.successCodes[ i ];

				ofs << " }";
			}
			ofs << " );\n";
		}
		else if( returnIndex != ~0 && commandData.returnType != returnType )
			ofs << indentation << "  return " << _reduceName( commandData.arguments[ returnIndex ].name ) << ";\n";

		ofs << indentation << "}\n";
	}
	//--------------------------------------------------------------------------
	void CppGenerator::_writeCall( std::ofstream& ofs, std::string const& name,
								   size_t templateIndex,
								   CommandData const& commandData,
								   std::set<std::string> const& vkTypes,
								   std::map<size_t, size_t> const& vectorParameters,
								   size_t returnIndex, bool firstCall ) const
	{
		std::map<size_t, size_t> countIndices;
		for( auto& it : vectorParameters )
			countIndices.insert( std::make_pair( it.second, it.first ) );

		if( vectorParameters.size() == 1
			&& ( commandData.arguments[ vectorParameters.begin()->first ].len == "dataSize/4"
				 || commandData.arguments[ vectorParameters.begin()->first ].len == "latexmath:[$dataSize \\over 4$]" ) )
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
				ofs << ", ";

			std::map<size_t, size_t>::const_iterator it = countIndices.find( i );
			if( it != countIndices.end() )
			{
				if( returnIndex == it->second && commandData.twoStep )
					ofs << "&" << _reduceName( commandData.arguments[ it->first ].name );

				else
				{
					ofs << _reduceName( commandData.arguments[ it->second ].name ) << ".size() ";
					if( templateIndex == it->second )
						ofs << "* sizeof( T ) ";
				}
			}
			else
			{
				it = vectorParameters.find( i );
				if( it != vectorParameters.end() )
				{
					assert( commandData.arguments[ it->first ].type.back() == '*' );
					if( returnIndex == it->first && commandData.twoStep && firstCall )
						ofs << "nullptr";

					else
					{
						auto vkit = vkTypes.find( commandData.arguments[ it->first ].pureType );
						if( vkit != vkTypes.end() || it->first == templateIndex )
						{
							ofs << "reinterpret_cast<";
							if( commandData.arguments[ it->first ].type.find( "const" ) != std::string::npos )
								ofs << "const ";

							if( vkit != vkTypes.end() )
								ofs << "Vk";

							ofs << commandData.arguments[ it->first ].pureType
								<< "*>( " << _reduceName( commandData.arguments[ it->first ].name ) << ".data() )";
						}
						else if( commandData.arguments[ it->first ].pureType == "char" )
						{
							ofs << _reduceName( commandData.arguments[ it->first ].name );
							if( commandData.arguments[ it->first ].optional )
								ofs << " ? " << _reduceName( commandData.arguments[ it->first ].name ) << "->c_str() : nullptr";

							else
								ofs << ".c_str()";
						}
						else
							ofs << _reduceName( commandData.arguments[ it->first ].name ) << ".data()";
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
								ofs << "static_cast<const " << commandData.arguments[ i ].pureType << "*>( ";
							else
								ofs << "&";

							ofs << _reduceName( commandData.arguments[ i ].name ) << ( commandData.arguments[ i ].optional ? ") )" : " )" );
						}
						else
						{
							assert( !commandData.arguments[ i ].optional );
							ofs << "reinterpret_cast<Vk" << commandData.arguments[ i ].pureType << "*>( &" << _reduceName( commandData.arguments[ i ].name ) << " )";
						}
					}
					else
						ofs << "static_cast<Vk" << commandData.arguments[ i ].pureType << ">( " << commandData.arguments[ i ].name << " )";
				}
				else
				{
					if( commandData.arguments[ i ].type.back() == '*' )
					{
						if( commandData.arguments[ i ].type.find( "const" ) != std::string::npos )
						{
							assert( commandData.arguments[ i ].type.find( "char" ) != std::string::npos );
							ofs << _reduceName( commandData.arguments[ i ].name );
							if( commandData.arguments[ i ].optional )
								ofs << " ? " << _reduceName( commandData.arguments[ i ].name ) << "->c_str() : nullptr";
							else
								ofs << ".c_str()";
						}
						else
						{
							assert( commandData.arguments[ i ].type.find( "char" ) == std::string::npos );
							ofs << "&" << _reduceName( commandData.arguments[ i ].name );
						}
					}
					else
						ofs << commandData.arguments[ i ].name;
				}
			}
		}
		ofs << " )";
	}
	//Unreferenced method
	/*void CppGenerator::_writeExceptionCheck( std::ofstream& ofs,
											 std::string const& indentation,
											 std::string const& className,
											 std::string const& functionName,
											 std::vector<std::string> const& successCodes )
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
				ofs << " ( result != Result::" << successCodes[ i ] << " ) &&";

			ofs << " ( result != Result::" << successCodes.back() << " )";
		}
		ofs << " )" << std::endl;
		ofs << indentation << "  {" << std::endl
			<< indentation << "    throw std::system_error( result, \"vk::";
		if( !className.empty() )
			ofs << className << "::";

		ofs << functionName << "\" );" << std::endl
			<< indentation << "  }" << std::endl;
	}*/
} // end of vk namespace
