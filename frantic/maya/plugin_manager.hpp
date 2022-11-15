// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/strings/tstring.hpp>

#include <maya/MPxData.h>
#include <maya/MPxNode.h>
#include <maya/MSceneMessage.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>
#if MAYA_API_VERSION >= 201200
#include <maya/MDrawRegistry.h>
#endif

#include <boost/smart_ptr.hpp>

#include <vector>

#if !( __APPLE__ && MAYA_API_VERSION >= 201800 )
class MFnPlugin;
#endif

namespace frantic {
namespace maya {

namespace detail {
////////////////////////////////////////
class plugin_registry_item;

////////////////////////////////////////
struct register_callback_t {
    typedef MMessage::MBasicFunction function_t;
    static MCallbackId add( MSceneMessage::Message msg, function_t func, void* clientData, MStatus* status ) {
        return MSceneMessage::addCallback( msg, func, clientData, status );
    }

    static MStatus remove( MCallbackId id ) { return MSceneMessage::removeCallback( id ); }
};

struct register_check_callback_t {
    typedef MMessage::MCheckFunction function_t;
    static MCallbackId add( MSceneMessage::Message msg, function_t func, void* clientData, MStatus* status ) {
        return MSceneMessage::addCheckCallback( msg, func, clientData, status );
    }

    static MStatus remove( MCallbackId id ) { return MSceneMessage::removeCallback( id ); }
};

struct register_check_file_callback_t {
    typedef MMessage::MCheckFileFunction function_t;
    static MCallbackId add( MSceneMessage::Message msg, function_t func, void* clientData, MStatus* status ) {
        return MSceneMessage::addCheckFileCallback( msg, func, clientData, status );
    }
    static MStatus remove( MCallbackId id ) { return MSceneMessage::removeCallback( id ); }
};

struct register_string_array_callback_t {
    typedef MMessage::MStringArrayFunction function_t;
    static MCallbackId add( MSceneMessage::Message msg, function_t func, void* clientData, MStatus* status ) {
        return MSceneMessage::addStringArrayCallback( msg, func, clientData, status );
    }
    static MStatus remove( MCallbackId id ) { return MSceneMessage::removeCallback( id ); }
};

} // namespace detail

////////////////////////////////////////
class plugin_manager {
  public:
    plugin_manager();
    ~plugin_manager();

    MStatus initialize( MObject pluginObject, const frantic::tstring& vendorName, const frantic::tstring& versionNumber,
                        const frantic::tstring& requiredAPIVersion );

    template <typename T>
    MStatus register_callback( MSceneMessage::Message msg, typename T::function_t func, void* clientData = NULL );
    MStatus register_command( const MString& commandName, MCreatorFunction creator,
                              MCreateSyntaxFunction createSyntaxFunction = NULL );
    MStatus register_data( const MString& typeName, const MTypeId& typeId, MCreatorFunction creatorFunction,
                           MPxData::Type type = MPxData::kData );
    MStatus register_mel_scripts( const frantic::tstring& initScript, const frantic::tstring& deinitScript,
                                  const frantic::tstring& description = _T("") );
    MStatus register_mel_script_files( const frantic::tstring& initScriptFile, const frantic::tstring& deinitScriptFile,
                                       const frantic::tstring& description = _T("") );
    MStatus register_python_scripts( const frantic::tstring& initScript, const frantic::tstring& deinitScript,
                                     const frantic::tstring& description = _T("") );
    MStatus register_shape( const MString& nodeName, MTypeId typeId, MCreatorFunction nodeCreator,
                            MInitializeFunction nodeInitializer, MCreatorFunction nodeUICreator,
                            const MString* classification = 0 );
    MStatus register_node( const MString& nodeName, MTypeId typeId, MCreatorFunction nodeCreator,
                           MInitializeFunction nodeInitializer, MPxNode::Type nodeType,
                           const MString* classification = 0 );
    MStatus register_ui( const MString& creationProc, const MString& deletionProc,
                         const MString& creationBatchProc = _T(""), const MString& deletionBatchProc = _T("") );
#if MAYA_API_VERSION >= 201200
    MStatus register_geometry_override_creator( const MString& drawClassification, const MString& registrantId,
                                                MHWRender::MDrawRegistry::GeometryOverrideCreator creator );
#endif

    MStatus unregister_all();

    bool is_loaded() const;

    frantic::tstring get_plugin_path() const;

  private:
    MStatus add_registry_item( detail::plugin_registry_item* item );
    boost::shared_ptr<MFnPlugin> m_plugin;
    typedef std::vector<boost::shared_ptr<detail::plugin_registry_item>> plugin_registry_vector;
    plugin_registry_vector m_registeredItems;
};

} // namespace maya
} // namespace frantic
