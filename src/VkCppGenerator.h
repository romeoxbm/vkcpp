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
#ifndef VKCPPGENERATOR
#define VKCPPGENERATOR

#include <tinyxml2.h>
#include <sstream>
#include <map>
#include <cassert>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <set>
#include <string>
#include <vector>
#include <exception>

namespace vk
{
	class VkCppGenerator
	{
	private:
		struct MemberData
		{
			std::string type;
			std::string name;
			std::string arraySize;
			std::string pureType;
			std::string len;
			bool		optional;
		};

		struct CommandData
		{
			CommandData()
			: handleCommand( false ),
			  twoStep( false )
			{}

			std::string					returnType;
			std::vector<MemberData>		arguments;
			std::vector<std::string>	successCodes;
			std::string					protect;
			bool						handleCommand;
			bool						twoStep;
		};

		struct DependencyData
		{
			enum class Category
			{
				COMMAND,
				ENUM,
				FLAGS,
				FUNC_POINTER,
				HANDLE,
				REQUIRED,
				SCALAR,
				STRUCT,
				UNION
			};

			DependencyData( Category c, std::string const& n )
			: category( c ),
			  name( n )
			{}

			Category				category;
			std::string				name;
			std::set<std::string>	dependencies;
		};

		struct NameValue
		{
			std::string name;
			std::string value;
		};

		struct EnumData
		{
			bool					bitmask;
			std::string				prefix;
			std::string				postfix;
			std::vector<NameValue>	members;
			std::string				protect;

			void addEnum( std::string const& name, std::string const& tag, bool appendTag );
		};

		struct FlagData
		{
			std::string protect;
		};

		struct HandleData
		{
			std::vector<std::string> commands;
		};

		struct ScalarData
		{
			std::string protect;
		};

		struct StructData
		{
			StructData()
			: returnedOnly( false )
			{}

			bool					returnedOnly;
			std::vector<MemberData>	members;
			std::string				protect;
		};

		struct VkData
		{
			std::map<std::string, CommandData>	commands;
			std::list<DependencyData>			dependencies;
			std::map<std::string, EnumData>		enums;
			std::map<std::string, FlagData>		flags;
			std::map<std::string, HandleData>	handles;
			std::map<std::string, ScalarData>	scalars;
			std::map<std::string, StructData>	structs;
			std::set<std::string>				tags;
			std::string							typesafeCheck;
			std::string							version;
			std::set<std::string>				vkTypes;
			std::string							vulkanLicenseHeader;
		};

		int parseFile( const std::string& filename );

		void createDefaults( VkData const& vkData,
							 std::map<std::string,std::string>& defaultValues );

		std::string determineFunctionName( std::string const& name,
										   CommandData const& commandData );

		std::string determineReturnType( CommandData const& commandData,
										 size_t returnIndex, bool isVector = false );

		void enterProtect( std::ofstream& ofs, std::string const& protect );

		std::string extractTag( std::string const& name );

		size_t findReturnIndex( CommandData const& commandData,
								std::map<size_t,size_t> const& vectorParameters );

		std::string findTag( std::string const& name,
							 std::set<std::string> const& tags );

		size_t findTemplateIndex( CommandData const& commandData,
								  std::map<size_t, size_t> const& vectorParameters );

		std::string generateEnumNameForFlags( std::string const& name );

		std::map<size_t, size_t> getVectorParameters( CommandData const& commandData );

		bool hasPointerArguments( CommandData const& commandData );

		bool isVectorSizeParameter( std::map<size_t, size_t> const& vectorParameters,
									size_t idx );
		void leaveProtect( std::ofstream& ofs, std::string const& protect );

		bool noDependencies( std::set<std::string> const& dependencies,
							 std::set<std::string>& listedTypes );

		bool readCommandParam( tinyxml2::XMLElement* element,
							   DependencyData& typeData,
							   std::vector<MemberData>& arguments );

		std::map<std::string, CommandData>::iterator readCommandProto(
				tinyxml2::XMLElement* element, VkData& vkData );

		void readCommands( tinyxml2::XMLElement* element, VkData& vkData );

		void readCommandsCommand( tinyxml2::XMLElement* element, VkData& vkData );

		void readComment( tinyxml2::XMLElement* element, std::string& header );

		void readEnums( tinyxml2::XMLElement* element, VkData& vkData );

		void readEnumsEnum( tinyxml2::XMLElement* element, EnumData& enumData,
							std::string const& tag );

		void readExtensionRequire( tinyxml2::XMLElement* element, VkData& vkData,
								   std::string const& protect,
								   std::string const& tag );

		void readExtensions( tinyxml2::XMLElement* element, VkData& vkData );

		void readExtensionsExtension( tinyxml2::XMLElement* element, VkData& vkData );

		void readTypeBasetype( tinyxml2::XMLElement* element,
							   std::list<DependencyData>& dependencies );

		void readTypeBitmask( tinyxml2::XMLElement* element, VkData& vkData );

		void readTypeDefine( tinyxml2::XMLElement* element, VkData& vkData );

		void readTypeFuncpointer( tinyxml2::XMLElement* element,
								  std::list<DependencyData>& dependencies );

		void readTypeHandle( tinyxml2::XMLElement* element, VkData& vkData );

		void readTypeStruct( tinyxml2::XMLElement* element, VkData& vkData );

		void readTypeStructMember( tinyxml2::XMLElement* element,
								   std::vector<MemberData>& members,
								   std::set<std::string>& dependencies );

		void readTypeUnion( tinyxml2::XMLElement* element, VkData& vkData );

		void readTypeUnionMember( tinyxml2::XMLElement* element,
								  std::vector<MemberData>& members,
								  std::set<std::string>& dependencies );

		void readTags( tinyxml2::XMLElement* element, std::set<std::string>& tags );

		void readTypes( tinyxml2::XMLElement* element, VkData & vkData );

		void sortDependencies( std::list<DependencyData>& dependencies );

		std::string reduceName( std::string const& name );

		static std::string strip( std::string const& value, std::string const& prefix,
						   std::string const& tag = std::string() );

		static std::string getEnumName( std::string const& name );

		static std::string stripCommand( std::string const& value );

		static std::string toCamelCase( std::string const& value );

		static std::string toUpperCase( std::string const& name );

		static std::string trimEnd( std::string const& input );

		void writeCall( std::ofstream& ofs, std::string const& name,
						size_t templateIndex,
						CommandData const& commandData,
						std::set<std::string> const& vkTypes,
						std::map<size_t, size_t> const& vectorParameters,
						size_t returnIndex, bool firstCall );

		void writeEnumsToString( std::ofstream& ofs, VkData const& vkData );

		void writeEnumsToString( std::ofstream& ofs, DependencyData const& dependencyData,
								 EnumData const& enumData );

		void writeFlagsToString( std::ofstream& ofs, DependencyData const& dependencyData,
								 EnumData const& enumData );

		void writeExceptionCheck( std::ofstream& ofs, std::string const& indentation,
								  std::string const& className,
								  std::string const& functionName,
								  std::vector<std::string> const& successCodes );

		void writeFunctionBody( std::ofstream& ofs,
								std::string const& indentation,
								std::string const& className,
								std::string const& functionName,
								std::string const& returnType,
								size_t templateIndex,
								DependencyData const& dependencyData,
								CommandData const& commandData,
								std::set<std::string> const& vkTypes,
								size_t returnIndex,
								std::map<size_t, size_t> const& vectorParameters );

		void writeFunctionHeader( std::ofstream& ofs, VkData const& vkData,
								  std::string const& indentation,
								  std::string const& returnType,
								  std::string const& name,
								  CommandData const& commandData,
								  size_t returnIndex,
								  size_t templateIndex,
								  std::map<size_t, size_t> const& vectorParameters );

		void writeMemberData( std::ofstream& ofs, MemberData const& memberData,
							  std::set<std::string> const& vkTypes );

		void writeStructConstructor( std::ofstream& ofs,
									 std::string const& name,
									 StructData const& structData,
									 std::set<std::string> const& vkTypes,
									 std::map<std::string,std::string> const&
									 defaultValues );

		void writeStructSetter( std::ofstream& ofs, std::string const& name,
								MemberData const& memberData,
								std::set<std::string> const& vkTypes//,
								/*std::map<std::string,StructData> const& structs*/ );

		void writeTypeCommand( std::ofstream& ofs, VkData const& vkData,
							   DependencyData const& dependencyData );

		void writeTypeCommandEnhanced( std::ofstream& ofs, VkData const& vkData,
									   std::string const& indentation,
									   std::string const& className,
									   std::string const& functionName,
									   DependencyData const& dependencyData,
									   CommandData const& commandData );

		void writeTypeCommandStandard( std::ofstream& ofs,
									   std::string const& indentation,
									   std::string const& functionName,
									   DependencyData const& dependencyData,
									   CommandData const& commandData,
									   std::set<std::string> const& vkTypes );

		void writeTypeEnum( std::ofstream& ofs, DependencyData const& dependencyData,
							EnumData const& enumData );

		void writeTypeFlags( std::ofstream& ofs, DependencyData const& dependencyData,
							 FlagData const& flagData );

		void writeTypeHandle( std::ofstream& ofs, VkData const& vkData,
							 DependencyData const& dependencyData,
							 HandleData const& handle,
							 std::list<DependencyData> const& dependencies );

		void writeTypeScalar( std::ofstream& ofs, DependencyData const& dependencyData );

		void writeTypeStruct( std::ofstream& ofs, VkData const& vkData,
							  DependencyData const& dependencyData,
							  std::map<std::string,std::string> const& defaultValues );

		void writeTypeUnion( std::ofstream& ofs, VkData const& vkData,
							 DependencyData const& dependencyData,
							 StructData const& unionData,
							 std::map<std::string,std::string> const& defaultValues );

		void writeTypes( std::ofstream& ofs, VkData const& vkData,
						 std::map<std::string, std::string> const& defaultValues );

		void writeVersionCheck( std::ofstream& ofs, std::string const& version );

		void writeTypesafeCheck( std::ofstream& ofs, std::string const& typesafeCheck );
	};
} // end of vk namespace
#endif // VKCPPGENERATOR
