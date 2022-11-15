// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <maya/MFnDagNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MPlug.h>

#include <frantic/maya/convert.hpp>
#include <frantic/maya/util.hpp>

namespace frantic {
namespace maya {

bool get_object_world_matrix( const MDagPath& dagNodePath, const MDGContext& currentContext,
                              frantic::graphics::transform4f& outTransform ) {
    MStatus status;
    MFnDagNode fnNode( dagNodePath, &status );

    if( !status )
        return false;

    MPlug worldTFormPlug = fnNode.findPlug( "worldMatrix", &status );

    if( !status )
        return false;

    MPlug matrixPlug = worldTFormPlug.elementByLogicalIndex( dagNodePath.instanceNumber(), &status );

    if( !status )
        return false;

    MObject matrixObject;
    status = matrixPlug.getValue( matrixObject, const_cast<MDGContext&>( currentContext ) );

    if( !status )
        return false;

    MFnMatrixData worldMatrixData( matrixObject );
    MMatrix mayaTransform = worldMatrixData.matrix();
    outTransform = frantic::maya::from_maya_t( mayaTransform );
    return true;
}

scoped_wait_cursor::scoped_wait_cursor() {
    int result;
    MGlobal::executeCommand( "waitCursor -q -state", result );
    m_setWaitCursor = !result;
    if( m_setWaitCursor ) {
        MGlobal::executeCommand( "waitCursor -state true" );
    }
}

scoped_wait_cursor::~scoped_wait_cursor() {
    if( m_setWaitCursor ) {
        MGlobal::executeCommand( "waitCursor -state false" );
    }
}

} // namespace maya
} // namespace frantic
