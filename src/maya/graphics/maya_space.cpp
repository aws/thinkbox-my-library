// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <frantic/maya/graphics/maya_space.hpp>

using namespace frantic::graphics;

namespace frantic {
namespace maya {
namespace graphics {

const transform4f ToMayaSpace( 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                               0.0f, 1.0f );
const transform4f FromMayaSpace( 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f );

} // namespace graphics
} // namespace maya
} // namespace frantic
