// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/convert.hpp>
#include <frantic/maya/shelf.hpp>

#include <maya/MGlobal.h>

#include <sstream>

namespace frantic {
namespace maya {

frantic::tstring get_current_shelf() {
    MString result;
    MGlobal::executeCommand( "global string $gShelfTopLevel; tabLayout -q -selectTab $gShelfTopLevel;", result );
    return from_maya_t( result );
}

bool shelf_exists( const frantic::tstring& shelfName ) {
    std::ostringstream command;
    command << "shelfLayout -exists \"" << frantic::strings::to_string( shelfName ).c_str() << "\"";
    int result;
    MGlobal::executeCommand( command.str().c_str(), result );
    return result ? true : false;
}

void clear_shelf( const frantic::tstring& shelfName ) {
    std::ostringstream listChildrenCommand;
    listChildrenCommand << "shelfLayout -q -childArray \"" << frantic::strings::to_string( shelfName ).c_str()
                        << "\";\n";
    MStringArray results;
    MGlobal::executeCommand( listChildrenCommand.str().c_str(), results );

    for( unsigned int i = 0; i < results.length(); ++i ) {
        std::ostringstream deleteButtonCommand;
        deleteButtonCommand << "deleteUI \"" << results[i] << "\";\n";
        MGlobal::executeCommand( deleteButtonCommand.str().c_str() );
    }
}

void delete_shelf( const frantic::tstring& shelfName ) {
    // It is important to clear the set of shelf icons before deleting for some reason
    clear_shelf( shelfName );

    std::ostringstream deleteShelfCommand;
    deleteShelfCommand << "deleteUI \"" << frantic::strings::to_string( shelfName ).c_str() << "\";\n";
    MGlobal::executeCommand( deleteShelfCommand.str().c_str() );
}

void create_shelf( const frantic::tstring& shelfName ) {
    std::ostringstream command;
    command << "addNewShelfTab " << frantic::strings::to_string( shelfName ).c_str() << ";\n";
    MGlobal::executeCommand( command.str().c_str() );
}

void switch_to_shelf( const frantic::tstring& shelfName ) {
    std::ostringstream command;
    command << "tabLayout -e -selectTab \"" << frantic::strings::to_string( shelfName ).c_str() << "\" $gShelfTopLevel";
    MGlobal::executeCommand( command.str().c_str() );
}

void create_shelf_button( const frantic::tstring& shelfName, const frantic::tstring& iconName,
                          const frantic::tstring& command, const frantic::tstring& toolTip,
                          const frantic::tstring& iconFilename ) {
    std::ostringstream commandBuffer;
    commandBuffer << "shelfButton -parent \"" << frantic::strings::to_string( shelfName ).c_str()
                  << "\" -enable 1 -width 34 -height 34 -manage 1 -visible 1 -label \""
                  << frantic::strings::to_string( iconName ).c_str() << "\" -annotation \""
                  << frantic::strings::to_string( toolTip ).c_str() << "\" -style \"iconOnly\" ";
    commandBuffer << " -command \"" << frantic::strings::to_string( command ).c_str() << "\"";

    if( iconFilename.length() > 0 ) {
        commandBuffer << " -image1 \"" << frantic::strings::to_string( iconFilename ) << "\"";
    }

    commandBuffer << ";";

    MGlobal::executeCommand( commandBuffer.str().c_str() );
}

} // namespace maya
} // namespace frantic
