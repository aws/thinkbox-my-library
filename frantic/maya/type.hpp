// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MSceneMessage.h>

#include <frantic/strings/tstring.hpp>

namespace frantic {
namespace maya {

frantic::tstring mscene_message_to_tstring( MSceneMessage::Message msg );

}
} // namespace frantic