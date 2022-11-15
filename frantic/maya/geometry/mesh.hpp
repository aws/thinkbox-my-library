// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MFnMesh.h>

#include <frantic/channels/channel_propagation_policy.hpp>
#include <frantic/geometry/polymesh3.hpp>
#include <frantic/geometry/trimesh3.hpp>

namespace frantic {
namespace maya {
namespace geometry {

/**
 * Create a polymesh3 object from a Maya mesh.
 * Does not produce velocity channel. Does not consider smooth mesh options.
 */
frantic::geometry::polymesh3_ptr polymesh_copy( const MDagPath& dagPath, bool worldSpace,
                                                const frantic::channels::channel_propagation_policy& cpp,
                                                bool colorFromCurrentColorSet = false,
                                                bool textureCoordFromCurrentUVSet = false );

/**
 * Uses the crease information stored in the edges of fnMesh to create an EdgeSharpness channel that is stored in
 * outMesh.
 */
void copy_edge_creases( const MDagPath& dagPath, const MFnMesh& srcMesh, frantic::geometry::polymesh3_ptr outMesh );

/**
 * Uses the crease information stored in the vertices of fnMesh to create a VertexSharpness channel that is stored in
 * outMesh.
 */
void copy_vertex_creases( const MDagPath& dagPath, const MFnMesh& srcMesh, frantic::geometry::polymesh3_ptr outMesh );

/**
 * Uses the smoothing information stored in the edges of fnMesh to create a SmoothingGroup channel that is stored in
 * outMesh.
 */
void create_smoothing_groups( const MFnMesh& fnMesh, frantic::geometry::polymesh3_ptr outMesh );

/**
 * Uses the smoothing information stored in the edges of fnMesh to create a SmoothingGroup channel that is stored in
 * outMesh. prevList is present for caching purposes
 */
void create_smoothing_groups( const MFnMesh& fnMesh, std::vector<boost::uint32_t>& prevEncoding,
                              frantic::geometry::polymesh3_ptr outMesh );

/**
 * Copies a maya mesh object into a trimesh3.
 * @param meshPlug The plug object that will be used to retrieve the mesh object. Generally the "outMesh" plug of a mesh
 * DAG node. The plug is needed because the mesh may be retireived at multiple times if velocities are being generated.
 * @param outMesh The output mesh that is to be created.
 * @param generateNormals If true, a "Normal" channel will be created and the normal information from the Maya mesh will
 * be copied into the frantic mesh.
 * @param generateVelocities If true, a "Velocity" channel will be created that contains vertex velocities. If there is
 * no vertex motion, no channel will be created.
 * @param generateColors If true, a "Color" channel will be created and the color information from the Maya mesh will be
 * copied into the frantic mesh.
 * @param useSmoothedMeshSubdivs If true, the mesh will respect the user's "smoothed mesh" subdivision options. The
 * subdivided mesh is used by renderers. If false, the base mesh will be returned.
 */
void copy_maya_mesh( MPlug meshPlug, frantic::geometry::trimesh3& outMesh, bool generateNormals, bool generateUVCoords,
                     bool generateVelocity, bool generateColors, bool useSmoothedMeshSubdivs );

/**
 * Copy a trimesh3 into a new Maya mesh.
 * @param[out] parentOrOwner parent of the mesh that will be created.
 * @param mesh the mesh to copy.
 */
void mesh_copy( MObject parentOrOwner, const frantic::geometry::trimesh3& mesh );

/**
 * Copy a trimesh3 into a new Maya mesh, with the vertices offset according
 * to the mesh's Velocity channel and the specified timeOffset.
 * @param[out] parentOrOwner parent of the mesh that will be created.
 * @param mesh the mesh to copy.
 * @param timeOffset the time offset used to move the vertices based on the
 *        Velocity channel of the mesh.
 */
void mesh_copy_time_offset( MObject parentOrOwner, const frantic::geometry::trimesh3& mesh, float timeOffset );

} // namespace geometry
} // namespace maya
} // namespace frantic
