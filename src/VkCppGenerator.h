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
#include "VkTextIndent.h"

namespace vk
{
	//Forward declaration
	class DualOFStream;

	class CppGenerator
	{
	public:
		struct Options
		{
			std::string inputFile;
			std::string outFileName;
			std::string outHeaderDirectory;
			std::string outSrcDirectory;
			std::string headerExt;
			std::string srcExt;
			std::string pch;
			std::string includeGuard;
			std::string cmdLine;
			char indentChar;
			unsigned short spaceSize = 1;
		};

		int generate( const Options& opt );

	private:
		TextIndent _indent;

		void _enterProtect( DualOFStream& ofs, std::string const& protect ) const;

		void _leaveProtect( DualOFStream& ofs, std::string const& protect ) const;

		void _enterProtect( std::ofstream& ofs, std::string const& protect ) const;

		void _leaveProtect( std::ofstream& ofs, std::string const& protect ) const;

		std::string _determineFunctionName( std::string const& name,
											CommandData const& commandData ) const;

		std::string _determineReturnType( CommandData const& commandData,
										  size_t returnIndex, bool isVector = false ) const;

		std::string _reduceName( std::string const& name ) const;

		size_t _findReturnIndex( CommandData const& commandData,
								 std::map<size_t, size_t> const& vectorParameters ) const;

		size_t _findTemplateIndex( CommandData const& commandData,
								   std::map<size_t, size_t> const& vectorParameters ) const;

		std::map<size_t, size_t> _getVectorParameters( CommandData const& commandData ) const;

		bool _hasPointerArguments( CommandData const& commandData ) const;

		bool _isVectorSizeParameter( std::map<size_t, size_t> const& vectorParameters,
									 size_t idx ) const;

		void _sortDependencies( std::list<DependencyData>& dependencies ) const;

		bool _noDependencies( std::set<std::string> const& dependencies,
							  std::set<std::string>& listedTypes ) const;

		void _createDefaults( SpecData* vkData,
							  std::map<std::string, std::string>& defaultValues ) const;

		//Write methods
		//----------------------------------------------------------------------
		void _writeVersionCheck( std::ofstream& ofs, std::string const& version ) const;

		void _writeTypesafeCheck( std::ofstream& ofs, std::string const& typesafeCheck ) const;

		void _writeEnumsToString( DualOFStream& ofs, SpecData* vkData );

		void _writeEnumsToString( DualOFStream& ofs,
								  DependencyData const& dependencyData,
								  EnumData const& enumData );

		void _writeFlagsToString( DualOFStream& ofs,
								  DependencyData const& dependencyData,
								  EnumData const& enumData );

		//Write types
		//----------------------------------------------------------------------
		void _writeTypes( DualOFStream& ofs, SpecData* vkData,
						  std::map<std::string, std::string> const& defaultValues );

		void _writeTypeCommand( std::ofstream& ofs, SpecData* vkData,
								DependencyData const& dependencyData ) const;

		void _writeTypeCommandStandard( std::ofstream& ofs,
										std::string const& indentation,
										std::string const& functionName,
										DependencyData const& dependencyData,
										CommandData const& commandData,
										std::set<std::string> const& vkTypes ) const;

		void _writeTypeCommandEnhanced( std::ofstream& ofs, SpecData* vkData,
										std::string const& indentation,
										std::string const& className,
										std::string const& functionName,
										DependencyData const& dependencyData,
										CommandData const& commandData ) const;

		void _writeTypeEnum( std::ofstream& ofs,
							 DependencyData const& dependencyData,
							 EnumData const& enumData );

		void _writeTypeFlags( DualOFStream& ofs,
							  DependencyData const& dependencyData,
							  FlagData const& flagData );

		void _writeTypeHandle( DualOFStream& ofs, SpecData* vkData,
							   DependencyData const& dependencyData,
							   HandleData const& handle,
							   std::list<DependencyData> const& dependencies );

		void _writeTypeScalar( std::ofstream& ofs,
							   DependencyData const& dependencyData ) const;

		void _writeTypeStruct( DualOFStream& ofs, SpecData* vkData,
							   DependencyData const& dependencyData,
							   std::map<std::string, std::string> const& defaultValues );

		void _writeTypeUnion( DualOFStream& ofs, SpecData* vkData,
							  DependencyData const& dependencyData,
							  StructData const& unionData,
							  std::map<std::string, std::string> const& defaultValues );

		//Structs
		//----------------------------------------------------------------------
		void _writeStructConstructor( DualOFStream& ofs,
									  std::string const& name,
									  StructData const& structData,
									  std::set<std::string> const& vkTypes,
									  std::map<std::string, std::string> const& defaultValues );

		void _writeStructSetter( DualOFStream& ofs, std::string const& name,
								 MemberData const& memberData,
								 std::set<std::string> const& vkTypes//,
								 /*std::map<std::string,StructData> const& structs*/ );

		//TypeCommandStandard
		//----------------------------------------------------------------------
		void _writeMemberData( std::ofstream& ofs, MemberData const& memberData,
							   std::set<std::string> const& vkTypes ) const;

		//TypeCommandEnhanced
		//----------------------------------------------------------------------
		void _writeFunctionHeader( std::ofstream& ofs, SpecData* vkData,
								   std::string const& indentation,
								   std::string const& returnType,
								   std::string const& name,
								   CommandData const& commandData,
								   size_t returnIndex,
								   size_t templateIndex,
								   std::map<size_t, size_t> const& vectorParameters ) const;

		void _writeFunctionBody( std::ofstream& ofs,
								 std::string const& indentation,
								 std::string const& className,
								 std::string const& functionName,
								 std::string const& returnType,
								 size_t templateIndex,
								 DependencyData const& dependencyData,
								 CommandData const& commandData,
								 std::set<std::string> const& vkTypes,
								 size_t returnIndex,
								 std::map<size_t, size_t> const& vectorParameters ) const;

		void _writeCall( std::ofstream& ofs, std::string const& name,
						 size_t templateIndex,
						 CommandData const& commandData,
						 std::set<std::string> const& vkTypes,
						 std::map<size_t, size_t> const& vectorParameters,
						 size_t returnIndex, bool firstCall ) const;

		void _writeComment( std::ofstream& ofs, std::string const& name,
							std::string const& type ) const;

		//Unreferenced method
		/*void _writeExceptionCheck( std::ofstream& ofs,
								   std::string const& indentation,
								   std::string const& className,
								   std::string const& functionName,
								   std::vector<std::string> const& successCodes );*/
	};
} // end of vk namespace
#endif // VKCPPGENERATOR_H
