// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <maya/MSceneMessage.h>

#include <frantic/strings/tstring.hpp>

namespace frantic {
namespace maya {

frantic::tstring mscene_message_to_tstring( MSceneMessage::Message msg ) {
    frantic::tstring ret = _T( "Unknown" );
    switch( msg ) {
    case MSceneMessage::kSceneUpdate:
        ret = _T( "kSceneUpdate" );
        break;
    case MSceneMessage::kBeforeNew:
        ret = _T( "MkBeforeNew" );
        break;
    case MSceneMessage::kAfterNew:
        ret = _T( "kAfterNew" );
        break;
    case MSceneMessage::kBeforeImport:
        ret = _T( "kBeforeImport" );
        break;
    case MSceneMessage::kAfterImport:
        ret = _T( "kAfterImport" );
        break;
    case MSceneMessage::kBeforeOpen:
        ret = _T( "kBeforeOpen" );
        break;
    case MSceneMessage::kAfterOpen:
        ret = _T( "kAfterOpen" );
        break;
#if MAYA_API_VERSION > 201300
    case MSceneMessage::kBeforeFileRead:
        ret = _T( "kBeforeFileRead" );
        break;
    case MSceneMessage::kAfterFileRead:
        ret = _T( "kAfterFileRead" );
        break;
#endif
#if MAYA_API_VERSION > 201500
    case MSceneMessage::kAfterSceneReadAndRecordEdits:
        ret = _T( "kAfterSceneReadAndRecordEdits" );
        break;
#endif
    case MSceneMessage::kBeforeExport:
        ret = _T( "kBeforeExport" );
        break;
    case MSceneMessage::kAfterExport:
        ret = _T( "kAfterExport" );
        break;
    case MSceneMessage::kBeforeSave:
        ret = _T( "kBeforeSave" );
        break;
    case MSceneMessage::kAfterSave:
        ret = _T( "kAfterSave" );
        break;
    case MSceneMessage::kBeforeReference:
        ret = _T( "kBeforeReference" );
        break;
    case MSceneMessage::kAfterReference:
        ret = _T( "kAfterReference" );
        break;
    case MSceneMessage::kBeforeRemoveReference:
        ret = _T( "kBeforeRemoveReference" );
        break;
    case MSceneMessage::kAfterRemoveReference:
        ret = _T( "kAfterRemoveReference" );
        break;
    case MSceneMessage::kBeforeImportReference:
        ret = _T( "kBeforeImportReference" );
        break;
    case MSceneMessage::kAfterImportReference:
        ret = _T( "kAfterImportReference" );
        break;
    case MSceneMessage::kBeforeExportReference:
        ret = _T( "kBeforeExportReference" );
        break;
    case MSceneMessage::kAfterExportReference:
        ret = _T( "kAfterExportReference" );
        break;
    case MSceneMessage::kBeforeUnloadReference:
        ret = _T( "kBeforeUnloadReference" );
        break;
    case MSceneMessage::kAfterUnloadReference:
        ret = _T( "kAfterUnloadReference" );
        break;
    case MSceneMessage::kBeforeSoftwareRender:
        ret = _T( "kBeforeSoftwareRender" );
        break;
    case MSceneMessage::kAfterSoftwareRender:
        ret = _T( "kAfterSoftwareRender" );
        break;
    case MSceneMessage::kBeforeSoftwareFrameRender:
        ret = _T( "kBeforeSoftwareFrameRender" );
        break;
    case MSceneMessage::kAfterSoftwareFrameRender:
        ret = _T( "kAfterSoftwareFrameRender" );
        break;
    case MSceneMessage::kSoftwareRenderInterrupted:
        ret = _T( "kSoftwareRenderInterrupted" );
        break;
    case MSceneMessage::kMayaInitialized:
        ret = _T( "kMayaInitialized" );
        break;
    case MSceneMessage::kMayaExiting:
        ret = _T( "kMayaExiting" );
        break;
    case MSceneMessage::kBeforeNewCheck:
        ret = _T( "kBeforeNewCheck" );
        break;
    case MSceneMessage::kBeforeOpenCheck:
        ret = _T( "kBeforeOpenCheck" );
        break;
    case MSceneMessage::kBeforeSaveCheck:
        ret = _T( "kBeforeSaveCheck" );
        break;
    case MSceneMessage::kBeforeImportCheck:
        ret = _T( "kBeforeImportCheck" );
        break;
    case MSceneMessage::kBeforeExportCheck:
        ret = _T( "kBeforeExportCheck" );
        break;
    case MSceneMessage::kBeforeLoadReference:
        ret = _T( "kBeforeLoadReference" );
        break;
    case MSceneMessage::kAfterLoadReference:
        ret = _T( "kAfterLoadReference" );
        break;
    case MSceneMessage::kBeforeLoadReferenceCheck:
        ret = _T( "kBeforeLoadReferenceCheck" );
        break;
    case MSceneMessage::kBeforeReferenceCheck:
        // MSceneMessage::kBeforeReferenceCheck =
        // MSceneMessage::kBeforeCreateReferenceCheck
        ret = _T( "kBeforeReferenceCheck" );
        break;
    case MSceneMessage::kBeforePluginLoad:
        ret = _T( "kBeforePluginLoad" );
        break;
    case MSceneMessage::kAfterPluginLoad:
        ret = _T( "kAfterPluginLoad" );
        break;
    case MSceneMessage::kBeforePluginUnload:
        ret = _T( "kBeforePluginUnload" );
        break;
    case MSceneMessage::kAfterPluginUnload:
        ret = _T( "kAfterPluginUnload" );
        break;
    case MSceneMessage::kBeforeCreateReference:
        ret = _T( "kBeforeCreateReference" );
        break;
    case MSceneMessage::kAfterCreateReference:
        ret = _T( "kAfterCreateReference" );
        break;
    case MSceneMessage::kExportStarted:
        ret = _T( "kExportStarted" );
        break;
#if MAYA_API_VERSION > 201300
    case MSceneMessage::kBeforeLoadReferenceAndRecordEdits:
        ret = _T( "kBeforeLoadReferenceAndRecordEdits" );
        break;
    case MSceneMessage::kAfterLoadReferenceAndRecordEdits:
        ret = _T( "kAfterLoadReferenceAndRecordEdits" );
        break;
    case MSceneMessage::kBeforeCreateReferenceAndRecordEdits:
        ret = _T( "kBeforeCreateReferenceAndRecordEdits" );
        break;
    case MSceneMessage::kAfterCreateReferenceAndRecordEdits:
        ret = _T( "kAfterCreateReferenceAndRecordEdits" );
        break;
#endif
    case MSceneMessage::kLast:
        ret = _T( "kLast" );
        break;
    }
    return ret;
}

} // namespace maya
} // namespace frantic
