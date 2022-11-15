// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/strings/tstring.hpp>

namespace frantic {
namespace maya {

frantic::tstring get_current_shelf();
bool shelf_exists( const frantic::tstring& shelfName );
void clear_shelf( const frantic::tstring& shelfName );
void delete_shelf( const frantic::tstring& shelfName );
void create_shelf( const frantic::tstring& shelfName );
void switch_to_shelf( const frantic::tstring& shelfName );
void create_shelf_button( const frantic::tstring& shelfName, const frantic::tstring& iconName,
                          const frantic::tstring& command, const frantic::tstring& toolTip = _T(""),
                          const frantic::tstring& iconFileName = _T("") );

} // namespace maya
} // namespace frantic
