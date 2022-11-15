// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/files/paths.hpp>
#include <frantic/maya/PRTMayaParticle.hpp>
#include <frantic/maya/PRTObject_base.hpp>
#include <frantic/maya/maya_util.hpp>

#include <maya/MCommonRenderSettingsData.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnCamera.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MPxData.h>
#include <maya/MRenderUtil.h>
#include <maya/MSelectionList.h>

#include <frantic/maya/attributes.hpp>
#include <frantic/maya/convert.hpp>
#include <frantic/maya/graphics/maya_space.hpp>

#include <frantic/graphics/vector3.hpp>
#include <frantic/graphics/vector3f.hpp>

#include <boost/bimap.hpp>

#include <map>

using namespace frantic::graphics;

namespace frantic {
namespace maya {

namespace {

const int MAYA_PIX_FORMAT = 6;
const int MAYA_AVI_FORMAT = 23;
const int MAYA_CIN_FORMAT = 11;
const int MAYA_DDS_FORMAT = 35;
const int MAYA_EPS_FORMAT = 9;
const int MAYA_GIF_FORMAT = 0;
const int MAYA_JPEG_FORMAT = 8;
const int MAYA_IFF_FORMAT = 7;
const int MAYA_IFF_16BIT_FORMAT = 10;
const int MAYA_PSD_FORMAT = 31;
const int MAYA_PSD_LAYERED_FORMAT = 36;
const int MAYA_PNG_FORMAT = 32;
const int MAYA_YUV_FORMAT = 12;
const int MAYA_RLA_FORMAT = 2;
const int MAYA_SGI_FORMAT = 5;
const int MAYA_SGI_16BIT_FORMAT = 13;
const int MAYA_PIC_FORMAT = 1;
const int MAYA_TGA_FORMAT = 19;
const int MAYA_TIF_FORMAT = 3;
const int MAYA_TIF_16BIT_FORMAT = 4;
const int MAYA_BMP_FORMAT = 20;

typedef boost::bimap<int, frantic::tstring> maya_image_format_bimap_t;
maya_image_format_bimap_t MayaImageFormatBiMap;

void ensure_image_format_bimap_initialized() {
    static bool initialized = false;

    if( !initialized ) {
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_PIX_FORMAT, _T("pix") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_AVI_FORMAT, _T("avi") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_CIN_FORMAT, _T("cin") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_DDS_FORMAT, _T("dds") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_EPS_FORMAT, _T("eps") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_GIF_FORMAT, _T("gif") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_JPEG_FORMAT, _T("jpg") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_IFF_FORMAT, _T("iff") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_PSD_FORMAT, _T("psd") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_PNG_FORMAT, _T("png") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_YUV_FORMAT, _T("yuv") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_RLA_FORMAT, _T("rla") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_SGI_FORMAT, _T("sgi") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_SGI_FORMAT, _T("pic") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_TGA_FORMAT, _T("tga") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_TIF_FORMAT, _T("tif") ) );
        MayaImageFormatBiMap.insert( maya_image_format_bimap_t::value_type( MAYA_BMP_FORMAT, _T("bmp") ) );
        initialized = true;
    }
}

} // namespace

namespace maya_util {

frantic::tstring get_render_filename( const MDGContext& currentContext, const frantic::tstring& cameraName,
                                      const frantic::tstring& appendedName, const frantic::tstring& fileExtension ) {
    MString projectDir;
    MGlobal::executeCommand( "workspace -q -rootDirectory -shortName;", projectDir );
    MString imagesDir;
    MGlobal::executeCommand( "workspace -q -fileRuleEntry images;", imagesDir );
    MCommonRenderSettingsData commonRenderSettings;
    MRenderUtil::getCommonRenderSettings( commonRenderSettings );
    MString imageBaseName; // Rename to 'sceneName'?

    // Get the scene name.
    MGlobal::executeCommand( "file -q -shortName -sceneName;", imageBaseName );

    if( imageBaseName.length() != 0 ) {
        imageBaseName = imageBaseName.substring( 0, imageBaseName.length() - 4 );
    } else {
        imageBaseName = "untitled";
    }

    MTime currentTime;
    currentContext.getTime( currentTime );
    double frameNumber = currentTime.asUnits( MTime::uiUnit() );

    MString imageName = commonRenderSettings.getImageName( MCommonRenderSettingsData::kFullPathImage, frameNumber,
                                                           imageBaseName, cameraName.c_str(), fileExtension.c_str(),
                                                           MFnRenderLayer::currentLayer(), true );

    if( appendedName.length() != 0 ) {
        std::string path = imageName.asChar();
        std::string directory =
            frantic::files::ensure_trailing_pathseparator( frantic::files::directory_from_path( path ) );

        MString filename = commonRenderSettings.name;
        if( filename.length() == 0 ) // Filename defaults to the scene name
            filename = imageBaseName;

        int previousLength = filename.length();
        filename += MString( "_" ) + MString( appendedName.c_str() );

        imageName = MString( directory.c_str() ) + filename +
                    imageName.substring( (int)directory.length() + previousLength, imageName.length() );
    }

    return frantic::strings::to_tstring( imageName.asChar() );
}

int get_current_render_image_format() {
    MCommonRenderSettingsData commonRenderSettings;
    MRenderUtil::getCommonRenderSettings( commonRenderSettings );
    return commonRenderSettings.imageFormat;
}

bool get_image_format_extension( int mayaFormatId, frantic::tstring& outImageFileExtension ) {
    ensure_image_format_bimap_initialized();
    maya_image_format_bimap_t::left_const_iterator it = MayaImageFormatBiMap.left.find( mayaFormatId );

    if( it == MayaImageFormatBiMap.left.end() )
        return false;

    outImageFileExtension = it->second;
    return true;
}

bool get_image_format_id( const frantic::tstring& imageFileExtension, int& outMayaFormatId ) {
    ensure_image_format_bimap_initialized();
    maya_image_format_bimap_t::right_const_iterator it = MayaImageFormatBiMap.right.find( imageFileExtension );

    if( it == MayaImageFormatBiMap.right.end() )
        return false;

    outMayaFormatId = it->second;
    return true;
}

/**
 * Grabs the 'worldMatrix' of the given object at the specified dag path at the specified time.  The full dag path is
 * required to actually get a proper transform, since just an MObject can appear multiple times in the same scene under
 * different transforms.
 *
 * @param dagNodePath the path to the scene object
 * @param currentTime the scene time at which to get the transform
 * @param outTransform location where the world transform will be placed when retrieved
 * @return whether or not the world matrix could be retrieved
 */
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

void find_all_renderable_cameras( std::vector<MDagPath>& outNodes ) {
    outNodes.clear();

    for( MItDag iter( MItDag::kDepthFirst, MFn::kCamera ); !iter.isDone(); iter.next() ) {
        MDagPath dagPath;
        iter.getPath( dagPath );
        MFnDagNode cameraNode( dagPath );

        if( frantic::maya::get_boolean_attribute( cameraNode, "renderable" ) ) {
            outNodes.push_back( dagPath );
        }
    }
}

/**
 * Iterates over the scene, and retrieves the paths to all nodes that have all of the requested function set
 *
 * @param type the type of scene node to search for
 * @param outNodes the output list of all nodes found with the desired types
 */
void find_nodes_with_type( MFn::Type type, std::vector<MDagPath>& outNodes ) {
    outNodes.clear();

    for( MItDag iter( MItDag::kDepthFirst, type ); !iter.isDone(); iter.next() ) {
        MDagPath dagPath;
        iter.getPath( dagPath );
        outNodes.push_back( dagPath );
    }
}

/**
 * Iterates over the scene, and retrieves the paths to all nodes that have all of the specified MTypeId.
 * This is useful for finding all custom plugin nodes of a specific type
 *
 * @param type the type id of scene node to search for
 * @param outNodes the output list of all nodes found with the desired types
 */
void find_nodes_with_type_id( MTypeId typeId, std::vector<MDagPath>& outNodes ) {
    outNodes.clear();

    for( MItDag iter( MItDag::kDepthFirst ); !iter.isDone(); iter.next() ) {
        MStatus convertStatus;
        MObject exportObject = iter.currentItem();
        MFnDependencyNode fnNode( exportObject, &convertStatus );

        if( convertStatus ) {
            if( fnNode.typeId() == typeId ) {
                MDagPath dagPath;
                iter.getPath( dagPath );
                outNodes.push_back( dagPath );
            }
        }
    }
}

/**
 * Gets nodes with an outParticleStream attribute
 * Adds the dagpaths and corresponding dependency nodes to the given lists
 * @param outPaths DagPath of the dependency nodes found (see below)
 * @param outNodes Dependency Nodes with the given output stream attribute (see below)
 * @param outputStreamAttr Attribute to check for
 * @param isBeginning ParticleStreams may be linked with each other.  If isBeginning is true, place the beginning nodes
 * in outNodes.  Otherwise, place the end nodes in outNodes
 */
void find_nodes_with_output_stream( std::vector<MDagPath>& outPaths, std::vector<MObject>& outNodes, bool isBeginning,
                                    MString outputStreamAttr ) {
    outPaths.clear();
    outNodes.clear();
    MStatus status;

    for( MItDag iter( MItDag::kDepthFirst ); !iter.isDone(); iter.next() ) {
        MObject exportObject = iter.currentItem();
        MFnDependencyNode fnNode( exportObject, &status );
        if( status ) {

            // Verify the attribute
            if( PRTObjectBase::hasParticleStreamMPxData( fnNode, outputStreamAttr ) ) {
                MDagPath dagPath;
                iter.getPath( dagPath );

                if( isBeginning ) {
                    outPaths.push_back( dagPath );
                    outNodes.push_back( exportObject );
                } else {
                    outPaths.push_back( dagPath );
                    outNodes.push_back( PRTObjectBase::getEndOfStreamChain( fnNode, outputStreamAttr ) );
                }
                continue;
            }

            // Check if it's a maya particle system and get the wrapper
            MFnParticleSystem mayaParticleSystem( exportObject, &status );
            if( status ) {
                frantic::tstring systemName = frantic::maya::from_maya_t( mayaParticleSystem.particleName() );
                // deformed particles will show up with nondeformed particles, so that we only want
                // to display deformed particles case
                if( !mayaParticleSystem.isDeformedParticleShape( &status ) ) {
                    MObject deformedParticleShape = mayaParticleSystem.deformedParticleShape( &status );
                    /// current particleStream has its deformed cases, we won't render it
                    if( deformedParticleShape != MObject::kNullObj ) {
                        MFnParticleSystem deformedParticleSystem( deformedParticleShape, &status );
                        if( status ) {
                            frantic::tstring deformedName =
                                frantic::maya::from_maya_t( deformedParticleSystem.particleName() );
                            if( deformedName != systemName ) {
                                continue;
                            }
                        } else {
                            continue;
                        }
                    }
                }

                // Get the corresponding wrapper particle if possible
                MObject prtmaya = PRTMayaParticle::getPRTMayaParticleFromMayaParticleStreamCheckDeformed(
                    mayaParticleSystem, &status );
                if( status == MS::kSuccess ) {
                    MFnDependencyNode prtNode( prtmaya, &status );
                    if( status == MS::kSuccess ) {
                        MDagPath dagPath;
                        iter.getPath( dagPath );
                        if( isBeginning ) {
                            outPaths.push_back( dagPath );
                            outNodes.push_back( prtmaya );
                        } else {
                            outPaths.push_back( dagPath );
                            outNodes.push_back( PRTObjectBase::getEndOfStreamChain( prtNode, outputStreamAttr ) );
                        }
                    }
                }
                continue;
            }
        }
    }
}

/**
 * Iterates over the scene, searching for a node with the specific name.  Only the first node
 * with that name will be returned
 *
 * @param name the name of the node to search for
 * @param outObject the MObject of the found node
 * @return true if a node with that name was found, false otherwise
 */
bool find_node( const MString& name, MObject& outObject ) {
    MSelectionList list;
    MStatus status;
    status = MGlobal::getSelectionListByName( name, list );

    if( !status )
        return false;

    status = list.getDependNode( 0, outObject );

    if( !status )
        return false;

    return true;
}

/**
 * Gets the current time
 */
MTime get_current_time() {
    double result;
    MGlobal::executeCommand( "currentTime -q;", result );
    MTime currentTime( result, MTime::uiUnit() );
    return currentTime;
}

/**
 * Gets the full name of the given maya node including dag paths
 */
MString get_node_full_name( const MFnDagNode& node ) {
    MString name;
    for( unsigned int i = 0; i < node.parentCount(); i++ ) {
        MObject obj = node.parent( i );
        MFnDagNode parent( obj );

        name = name + parent.name() + "|";
    }
    name = name + node.name();
    return name;
}

/**
 * Gets the full name of the given maya node including dag paths
 */
MString get_node_full_name( const MFnDependencyNode& node ) {
    MStatus status;
    MFnDagNode dag( node.object(), &status );
    if( status == MS::kSuccess )
        return get_node_full_name( dag );
    return node.name();
}

} // namespace maya_util

} // namespace maya
} // namespace frantic
