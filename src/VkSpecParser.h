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
#ifndef VKSPECPARSER_H
#define VKSPECPARSER_H

#include <tinyxml2.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <list>

namespace vk
{
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

		void addEnum( std::string const& name, std::string const& value,
					  std::string const& tag,
					  bool appendTag );
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

	struct SpecData
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

	class SpecParser
	{
	public:
		SpecParser();

		SpecData* parse( const std::string& filename ) const;

	private:
		/**
		 * @brief get vkcpp enum name from vk enum name.
		 */
		std::string _getEnumName( std::string const& name ) const;

		std::string _getEnumValue( tinyxml2::XMLElement* element ) const;

		std::string _stripCommand( std::string const& value ) const;

		std::string _findTag( std::string const& name,
			std::set<std::string> const& tags ) const;

		std::string _extractTag( std::string const& name ) const;

		std::string _generateEnumNameForFlags( std::string const& name ) const;

		void _readCommands( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readCommandsCommand( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		std::map<std::string, CommandData>::iterator _readCommandProto(
			tinyxml2::XMLElement* element, SpecData* vkData ) const;

		bool _readCommandParam( tinyxml2::XMLElement* element,
			DependencyData& typeData,
			std::vector<MemberData>& arguments ) const;

		void _readComment( tinyxml2::XMLElement* element, std::string& header ) const;

		void _readEnums( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readEnumsEnum( tinyxml2::XMLElement* element, EnumData& enumData,
			std::string const& tag ) const;

		void _readExtensions( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readExtensionsExtension( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readExtensionRequire( tinyxml2::XMLElement* element, SpecData* vkData,
			std::string const& protect,
			std::string const& tag ) const;

		void _readTags( tinyxml2::XMLElement* element, std::set<std::string>& tags ) const;

		void _readTypes( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readTypeBasetype( tinyxml2::XMLElement* element,
			std::list<DependencyData>& dependencies ) const;

		void _readTypeBitmask( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readTypeDefine( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readTypeFuncpointer( tinyxml2::XMLElement* element,
			std::list<DependencyData>& dependencies ) const;

		void _readTypeHandle( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readTypeStruct( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readTypeStructMember( tinyxml2::XMLElement* element,
			std::vector<MemberData>& members,
			std::set<std::string>& dependencies ) const;

		void _readTypeUnion( tinyxml2::XMLElement* element, SpecData* vkData ) const;

		void _readTypeUnionMember( tinyxml2::XMLElement* element,
			std::vector<MemberData>& members,
			std::set<std::string>& dependencies ) const;
	};
}
#endif // VKSPECPARSER_H
