// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/graphics/transform4f.hpp>
#include <frantic/graphics/vector3f.hpp>

#include <frantic/maya/convert.hpp>

namespace frantic {
namespace maya {
namespace graphics {

extern const frantic::graphics::transform4f ToMayaSpace;
extern const frantic::graphics::transform4f FromMayaSpace;

inline frantic::graphics::vector3f from_maya_space( const frantic::graphics::vector3f& mayaSpaceVector ) {
    return frantic::graphics::vector3f( mayaSpaceVector.x, -mayaSpaceVector.z, mayaSpaceVector.y );
}

inline frantic::graphics::vector3f to_maya_space( const frantic::graphics::vector3f& vector ) {
    return frantic::graphics::vector3f( vector.x, vector.z, -vector.y );
}

} // namespace graphics
} // namespace maya
} // namespace frantic
