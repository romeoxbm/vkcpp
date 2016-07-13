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
#ifndef VKCPPGENERATOR_H
#define VKCPPGENERATOR_H

#include "VkSpecParser.h"

namespace vk
{
	class CppGenerator
	{
	public:
		struct Options
		{
			std::string inputFile;
			std::string outFileName;
			std::string outDirectory;
			std::string includeGuard;
		};

		int generate( const Options& opt );

	private:
		void createDefaults( SpecData* vkData,
			std::map<std::string, std::string>& defaultValues );

		std::string determineFunctionName( std::string const& name,
			CommandData const& commandData );

		std::string determineReturnType( CommandData const& commandData,
			size_t returnIndex, bool isVector = false );

		void enterProtect( std::ofstream& ofs, std::string const& protect );

		size_t findReturnIndex( CommandData const& commandData,
			std::map<size_t, size_t> const& vectorParameters );

		size_t findTemplateIndex( CommandData const& commandData,
			std::map<size_t, size_t> const& vectorParameters );

		std::map<size_t, size_t> getVectorParameters( CommandData const& commandData );

		bool hasPointerArguments( CommandData const& commandData );

		bool isVectorSizeParameter( std::map<size_t, size_t> const& vectorParameters,
			size_t idx );
		void leaveProtect( std::ofstream& ofs, std::string const& protect );

		bool noDependencies( std::set<std::string> const& dependencies,
			std::set<std::string>& listedTypes );

		void sortDependencies( std::list<DependencyData>& dependencies );

		std::string reduceName( std::string const& name );

		void writeCall( std::ofstream& ofs, std::string const& name,
			size_t templateIndex,
			CommandData const& commandData,
			std::set<std::string> const& vkTypes,
			std::map<size_t, size_t> const& vectorParameters,
			size_t returnIndex, bool firstCall );

		void writeEnumsToString( std::ofstream& ofs, SpecData* vkData );

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

		void writeFunctionHeader( std::ofstream& ofs, SpecData* vkData,
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
			std::map<std::string, std::string> const&
			defaultValues );

		void writeStructSetter( std::ofstream& ofs, std::string const& name,
			MemberData const& memberData,
			std::set<std::string> const& vkTypes//,
			/*std::map<std::string,StructData> const& structs*/ );

		void writeTypeCommand( std::ofstream& ofs, SpecData* vkData,
			DependencyData const& dependencyData );

		void writeTypeCommandEnhanced( std::ofstream& ofs, SpecData* vkData,
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

		void writeTypeHandle( std::ofstream& ofs, SpecData* vkData,
			DependencyData const& dependencyData,
			HandleData const& handle,
			std::list<DependencyData> const& dependencies );

		void writeTypeScalar( std::ofstream& ofs, DependencyData const& dependencyData );

		void writeTypeStruct( std::ofstream& ofs, SpecData* vkData,
			DependencyData const& dependencyData,
			std::map<std::string, std::string> const& defaultValues );

		void writeTypeUnion( std::ofstream& ofs, SpecData* vkData,
			DependencyData const& dependencyData,
			StructData const& unionData,
			std::map<std::string, std::string> const& defaultValues );

		void writeTypes(std::ofstream& ofs, SpecData* vkData,
			std::map<std::string, std::string> const& defaultValues );

		void writeVersionCheck( std::ofstream& ofs, std::string const& version );

		void writeTypesafeCheck( std::ofstream& ofs, std::string const& typesafeCheck );
	};
} // end of vk namespace
#endif // VKCPPGENERATOR_H
