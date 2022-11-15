// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/geometry/trimesh3.hpp>

namespace frantic {
namespace maya {
namespace graphics {

void gl_draw_wireframe( const frantic::geometry::trimesh3& mesh );
void gl_draw( const frantic::geometry::trimesh3& mesh );

void gl_draw_box_wireframe( const frantic::graphics::boundbox3f& box );

} // namespace graphics
} // namespace maya
} // namespace frantic
