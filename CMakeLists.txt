# Copyright(c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
cmake_minimum_required( VERSION 2.8 )
project( VkCppGenerator )

set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" )

#Make a version file containing the current version from git
include( GetGitRevisionDescription )
git_describe( VERSION --tags )

#Parse the version information into pieces.
string( REGEX REPLACE "^v([0-9]+)\\..*" "\\1" VERSION_MAJOR "${VERSION}" )
string( REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${VERSION}" )
string( REGEX REPLACE "^v[0-9]+\\.[0-9]+-([0-9]+).*" "\\1" VERSION_PATCH "${VERSION}" )
string( REGEX REPLACE "^v[0-9]+\\.[0-9]+-[0-9]+-(.*)" "\\1" VERSION_SHA1 "${VERSION}" )

if( ${VERSION_PATCH} STREQUAL ${VERSION} )
	set( VERSION_PATCH "0" )
endif()

set( VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/VkVersion.h.in
	${CMAKE_CURRENT_BINARY_DIR}/VkVersion.h
)

message( STATUS "Version: ${VERSION}" )

#Look for tinyxml2
find_package( tinyxml2 REQUIRED )
if( NOT TINYXML2_FOUND )
	message( FATAL_ERROR "* Failed to find tinyxml2." )
endif()

set( HEADERS
	src/cmdline.h
	src/Strings.h
	src/StringsHelper.h
	src/VkCppGenerator.h
	src/VkDualOFStream.h
	src/VkSpecParser.h
	src/VkTextIndent.h
	${CMAKE_CURRENT_BINARY_DIR}/VkVersion.h
)

set( SOURCES
	src/main.cpp
	src/StringsHelper.cpp
	src/VkCppGenerator.cpp
	src/VkDualOFStream.cpp
	src/VkSpecParser.cpp
	src/VkTextIndent.cpp
)

source_group( headers FILES ${HEADERS} )
source_group( sources FILES ${SOURCES} )

add_executable( VkCppGenerator
	${HEADERS}
	${SOURCES}
)

set_property( TARGET VkCppGenerator PROPERTY CXX_STANDARD 11 )
target_include_directories( VkCppGenerator
	PRIVATE ${TINYXML2_INCLUDE_DIR}
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries( VkCppGenerator
	PRIVATE ${TINYXML2_LIBRARIES}
)
