// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MAngle.h>
#include <maya/MDGContext.h>
#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>
#include <maya/MTypeId.h>

#include <frantic/graphics/transform4f.hpp>
#include <frantic/strings/tstring.hpp>

#include <boost/lexical_cast.hpp>

#include <vector>

namespace frantic {
namespace maya {

namespace maya_util {

inline double get_fps() { return MTime( 1.0, MTime::kSeconds ).as( MTime::uiUnit() ); }

void find_all_renderable_cameras( std::vector<MDagPath>& outNodes );

void find_nodes_with_type( MFn::Type type, std::vector<MDagPath>& outNodes );

void find_nodes_with_type_id( MTypeId typeId, std::vector<MDagPath>& outNodes );

void find_nodes_with_output_stream( std::vector<MDagPath>& outPaths, std::vector<MObject>& outNodes,
                                    bool isBeginning = true, MString outputStreamAttr = "outParticleStream" );

bool find_node( const MString& name, MObject& outObject );

bool get_object_world_matrix( const MDagPath& dagNodePath, const MDGContext& currentTime,
                              frantic::graphics::transform4f& outTransform );

frantic::tstring get_render_filename( const MDGContext& currentTime, const frantic::tstring& cameraName = _T(""),
                                      const frantic::tstring& appendedName = _T(""),
                                      const frantic::tstring& fileExtension = _T("") );

bool get_image_format_extension( int mayaFormatId, frantic::tstring& outImageFileExtension );

bool get_image_format_id( const frantic::tstring& imageFileExtension, int& outMayaFormatId );

int get_current_render_image_format();

MString get_node_full_name( const MFnDagNode& node );

MString get_node_full_name( const MFnDependencyNode& node );

MTime get_current_time();

} // namespace maya_util

} // namespace maya
} // namespace frantic
