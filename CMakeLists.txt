# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

project( ThinkboxMYLibrary )

find_package( thinkboxcmlibrary REQUIRED )
include( PrecompiledHeader )
include( ThinkboxCMLibrary )

set( SUPPORTED_MAYA_VERSIONS 2022 2023 )

option( MAYA_VERSION "The version of Maya SDK to build the library against." 2022 )

if( NOT MAYA_VERSION IN_LIST SUPPORTED_MAYA_VERSIONS )
  message( FATAL_ERROR "ERROR: Cannot build for unsupported Maya version ${MAYA_VERSION}" )
endif()

add_library( thinkboxmylibrary STATIC )

target_include_directories( thinkboxmylibrary PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

file( GLOB_RECURSE H_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "frantic/*.h"
  "frantic/*.hpp"
)

file( GLOB_RECURSE CXX_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "src/*.cpp"
)

target_sources( thinkboxmylibrary PRIVATE
	stdafx.cpp
	stdafx.h
  ${H_FILES}
  ${CXX_FILES}
)

# The Conan version of Boost was built with this, and it changes the library names.
# As a result, we need to set this to tell Boost to look for the right libraries to
# link against.
target_compile_definitions( thinkboxmylibrary PUBLIC BOOST_AUTO_LINK_SYSTEM )

find_package( thinkboxlibrary REQUIRED )
find_package( Boost REQUIRED )
find_package( Eigen3 REQUIRED )
find_package( OpenEXR REQUIRED )
find_package( ZLIB REQUIRED )
find_package( TBB REQUIRED )
find_package( tinyxml2 REQUIRED )
find_package( mayasdk REQUIRED )

target_include_directories( thinkboxmylibrary PUBLIC ${thinkboxlibrary_INCLUDE_DIRS} )
target_include_directories( thinkboxmylibrary PUBLIC ${Boost_INCLUDE_DIRS} )
target_include_directories( thinkboxmylibrary PUBLIC ${Eigen3_INCLUDE_DIRS} )
target_include_directories( thinkboxmylibrary PUBLIC ${OpenEXR_INCLUDE_DIRS} )
target_include_directories( thinkboxmylibrary PUBLIC ${ZLIB_INCLUDE_DIRS} )
target_include_directories( thinkboxmylibrary PUBLIC ${TBB_INCLUDE_DIRS} )
target_include_directories( thinkboxmylibrary PUBLIC ${tinyxml2_INCLUDE_DIRS} )
target_include_directories( thinkboxmylibrary PUBLIC ${mayasdk_INCLUDE_DIR} )

target_link_libraries( thinkboxmylibrary INTERFACE thinkboxlibrary::thinkboxlibrary )
target_link_libraries( thinkboxmylibrary INTERFACE Boost::Boost )
target_link_libraries( thinkboxmylibrary INTERFACE Eigen3::Eigen )
target_link_libraries( thinkboxmylibrary INTERFACE OpenEXR::OpenEXR )
target_link_libraries( thinkboxmylibrary INTERFACE ZLIB::ZLIB )
target_link_libraries( thinkboxmylibrary INTERFACE TBB::tbb )
target_link_libraries( thinkboxmylibrary INTERFACE tinyxml2::tinyxml2 )
target_link_libraries( thinkboxmylibrary INTERFACE mayasdk::mayasdk )

frantic_common_platform_setup( thinkboxmylibrary )
frantic_default_source_groups( thinkboxmylibrary HEADERDIR include SOURCEDIR src )

# Disable optimization for the RelWithDebInfo configuration on Windows.
# This allows breakpoints to be hit reliably when debugging in Visual Studio.
if( WIN32 )
	target_compile_options( thinkboxmylibrary PRIVATE "$<$<CONFIG:Release>:/O2>$<$<CONFIG:RelWithDebInfo>:/Od>" )
endif()
  
install( DIRECTORY frantic
          DESTINATION include
          FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
install( TARGETS thinkboxmylibrary
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib
)