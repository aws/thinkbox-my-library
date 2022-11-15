// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

// Maya has this weird thing where you can't include MFnPlugin more than once, since it defines a bunch of crap in your
// dll main These defines get around that
#define MNoPluginEntry
#define MNoVersionString
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>

#include <frantic/maya/convert.hpp>
#include <frantic/maya/plugin_manager.hpp>
#include <frantic/maya/type.hpp>

namespace {

frantic::tstring make_mel_source_call( const frantic::tstring& scriptPath ) {
    return _T( "source \"" ) + scriptPath + _T( "\";" );
}

} // namespace

namespace frantic {
namespace maya {

namespace detail {

class plugin_registry_item {
  public:
    virtual MStatus init( MFnPlugin& plugin ) = 0;
    virtual MStatus deinit( MFnPlugin& plugin ) = 0;
    virtual frantic::tstring description() = 0;
};

template <typename T>
class plugin_callback_item : public plugin_registry_item {
  public:
    explicit plugin_callback_item( MSceneMessage::Message msg, typename T::function_t func, void* clientData )
        : m_message( msg )
        , m_function( func )
        , m_clientData( clientData )
        , m_callbackId() {}

    virtual MStatus init( MFnPlugin& /*plugin*/ ) {
        MStatus status;
        m_callbackId = T::add( m_message, static_cast<typename T::function_t>( m_function ), m_clientData, &status );
        return status;
    }

    virtual MStatus deinit( MFnPlugin& /*plugin*/ ) { return T::remove( m_callbackId ); }

    virtual frantic::tstring description() {
        return _T("Callback") + frantic::maya::mscene_message_to_tstring( m_message );
    }

  private:
    MSceneMessage::Message m_message;
    typename T::function_t m_function;
    void* m_clientData;
    MCallbackId m_callbackId;
};

class plugin_command_item : public plugin_registry_item {
  public:
    plugin_command_item( const MString& commandName, MCreatorFunction creator,
                         MCreateSyntaxFunction createSyntaxFunction )
        : m_commandName( commandName )
        , m_creator( creator )
        , m_createSyntaxFunction( createSyntaxFunction ) {}

    virtual MStatus init( MFnPlugin& plugin ) {
        return plugin.registerCommand( m_commandName, m_creator, m_createSyntaxFunction );
    }

    virtual MStatus deinit( MFnPlugin& plugin ) { return plugin.deregisterCommand( m_commandName ); }

    virtual frantic::tstring description() {
        return _T("Command ") + frantic::strings::to_tstring( m_commandName.asChar() );
    }

  private:
    MString m_commandName;
    MCreatorFunction m_creator;
    MCreateSyntaxFunction m_createSyntaxFunction;
};

class plugin_data_item : public plugin_registry_item {
  public:
    plugin_data_item( const MString& typeName, const MTypeId& typeId, MCreatorFunction creatorFunction,
                      MPxData::Type type )
        : m_typeName( typeName )
        , m_typeId( typeId )
        , m_creator( creatorFunction )
        , m_type( type ) {}

    virtual MStatus init( MFnPlugin& plugin ) { return plugin.registerData( m_typeName, m_typeId, m_creator, m_type ); }

    virtual MStatus deinit( MFnPlugin& plugin ) { return plugin.deregisterData( m_typeId ); }

    virtual frantic::tstring description() { return _T("Type ") + frantic::strings::to_tstring( m_typeName.asChar() ); }

  private:
    MString m_typeName;
    MTypeId m_typeId;
    MCreatorFunction m_creator;
    MPxData::Type m_type;
};

class plugin_node_item : public plugin_registry_item {
  public:
    plugin_node_item( const MString& nodeName, MTypeId typeId, MCreatorFunction nodeCreator,
                      MInitializeFunction nodeInitializer, MPxNode::Type nodeType, const MString* classification = 0 )
        : m_nodeName( nodeName )
        , m_typeId( typeId )
        , m_nodeCreator( nodeCreator )
        , m_nodeInitializer( nodeInitializer )
        , m_nodeType( nodeType )
        , m_hasClassification( false ) {
        if( classification ) {
            m_hasClassification = true;
            m_classification = *classification;
        }
    }

    virtual MStatus init( MFnPlugin& plugin ) {
        return plugin.registerNode( m_nodeName, m_typeId, m_nodeCreator, m_nodeInitializer, m_nodeType,
                                    m_hasClassification ? &m_classification : 0 );
    }

    virtual MStatus deinit( MFnPlugin& plugin ) { return plugin.deregisterNode( m_typeId ); }

    virtual frantic::tstring description() { return _T("Node ") + frantic::strings::to_tstring( m_nodeName.asChar() ); }

  private:
    MString m_nodeName;
    MTypeId m_typeId;
    MCreatorFunction m_nodeCreator;
    MInitializeFunction m_nodeInitializer;
    MPxNode::Type m_nodeType;
    bool m_hasClassification;
    MString m_classification;
};

class plugin_shape_item : public plugin_registry_item {
  public:
    plugin_shape_item( const MString& nodeName, MTypeId typeId, MCreatorFunction nodeCreator,
                       MInitializeFunction nodeInitializer, MCreatorFunction nodeUICreator,
                       const MString* classification = 0 )
        : m_nodeName( nodeName )
        , m_typeId( typeId )
        , m_nodeCreator( nodeCreator )
        , m_nodeInitializer( nodeInitializer )
        , m_nodeUICreator( nodeUICreator )
        , m_hasClassification( false ) {
        if( classification ) {
            m_hasClassification = true;
            m_classification = *classification;
        }
    }

    virtual MStatus init( MFnPlugin& plugin ) {
        return plugin.registerShape( m_nodeName, m_typeId, m_nodeCreator, m_nodeInitializer, m_nodeUICreator,
                                     m_hasClassification ? &m_classification : 0 );
    }

    virtual MStatus deinit( MFnPlugin& plugin ) { return plugin.deregisterNode( m_typeId ); }

    virtual frantic::tstring description() {
        return _T("Shape Node ") + frantic::strings::to_tstring( m_nodeName.asChar() );
    }

  private:
    MString m_nodeName;
    MTypeId m_typeId;
    MCreatorFunction m_nodeCreator;
    MInitializeFunction m_nodeInitializer;
    MCreatorFunction m_nodeUICreator;
    bool m_hasClassification;
    MString m_classification;
};

class plugin_mel_scripts_item : public plugin_registry_item {
  public:
    plugin_mel_scripts_item( const frantic::tstring& initScript, const frantic::tstring& deinitScript,
                             const frantic::tstring& /*description*/ )
        : m_initScript( initScript )
        , m_deinitScript( deinitScript ) {}

    virtual MStatus init( MFnPlugin& /*plugin*/ ) { return MGlobal::executeCommand( to_maya_t( m_initScript ) ); }

    virtual MStatus deinit( MFnPlugin& /*plugin*/ ) { return MGlobal::executeCommand( to_maya_t( m_deinitScript ) ); }

    virtual frantic::tstring description() {
        if( m_description.length() > 0 )
            return m_description;
        else
            return _T("Initialization Mel Scripts");
    }

  private:
    frantic::tstring m_initScript;
    frantic::tstring m_deinitScript;
    frantic::tstring m_description;
};

class plugin_python_scripts_item : public plugin_registry_item {
  public:
    plugin_python_scripts_item( const frantic::tstring& initScript, const frantic::tstring& deinitScript,
                                const frantic::tstring& /*description*/ )
        : m_initScript( initScript )
        , m_deinitScript( deinitScript ) {}

    virtual MStatus init( MFnPlugin& /*plugin*/ ) { return MGlobal::executePythonCommand( to_maya_t( m_initScript ) ); }

    virtual MStatus deinit( MFnPlugin& /*plugin*/ ) {
        return MGlobal::executePythonCommand( to_maya_t( m_deinitScript ) );
    }

    virtual frantic::tstring description() {
        if( m_description.length() > 0 )
            return m_description;
        else
            return _T("Initialization Python Scripts");
    }

  private:
    frantic::tstring m_initScript;
    frantic::tstring m_deinitScript;
    frantic::tstring m_description;
};

class plugin_ui_item : public plugin_registry_item {
  public:
    plugin_ui_item( const MString& creationProc, const MString& deletionProc, const MString& creationBatchProc = "",
                    const MString& deletionBatchProc = "" )
        : m_creationProc( creationProc )
        , m_deletionProc( deletionProc )
        , m_creationBatchProc( creationBatchProc )
        , m_deletionBatchProc( deletionBatchProc ) {}

    virtual MStatus init( MFnPlugin& plugin ) {
        return plugin.registerUI( m_creationProc, m_deletionProc, m_creationBatchProc, m_deletionBatchProc );
    }

    virtual MStatus deinit( MFnPlugin& plugin ) { return MStatus::kSuccess; }

    virtual frantic::tstring description() {
        return _T("UI ") + frantic::strings::to_tstring( m_creationProc.asChar() );
    }

  private:
    MString m_creationProc;
    MString m_deletionProc;
    MString m_creationBatchProc;
    MString m_deletionBatchProc;
};

#if MAYA_API_VERSION >= 201200

class plugin_geometry_override_item : public plugin_registry_item {
  public:
    plugin_geometry_override_item( const MString& drawClassification, const MString& registrantId,
                                   MHWRender::MDrawRegistry::GeometryOverrideCreator creator )
        : m_drawClassification( drawClassification )
        , m_registrantId( registrantId )
        , m_creator( creator ) {}

    virtual MStatus init( MFnPlugin& /*plugin*/ ) {
        return MHWRender::MDrawRegistry::registerGeometryOverrideCreator( m_drawClassification, m_registrantId,
                                                                          m_creator );
    }

    virtual MStatus deinit( MFnPlugin& /*plugin*/ ) {
        return MHWRender::MDrawRegistry::deregisterGeometryOverrideCreator( m_drawClassification, m_registrantId );
    }

    virtual frantic::tstring description() {
        return _T("Geometry Override ") + frantic::strings::to_tstring( m_drawClassification.asChar() );
    }

  private:
    MString m_drawClassification;
    MString m_registrantId;
    MHWRender::MDrawRegistry::GeometryOverrideCreator m_creator;
};

#endif

} // namespace detail

plugin_manager::plugin_manager() {}

plugin_manager::~plugin_manager() {}

MStatus plugin_manager::initialize( MObject pluginObject, const frantic::tstring& vendorName,
                                    const frantic::tstring& versionNumber,
                                    const frantic::tstring& requiredAPIVersion ) {
    MStatus outStatus;
    m_plugin = boost::shared_ptr<MFnPlugin>(
        new MFnPlugin( pluginObject, frantic::strings::to_string( vendorName ).c_str(),
                       frantic::strings::to_string( versionNumber ).c_str(),
                       frantic::strings::to_string( requiredAPIVersion ).c_str(), &outStatus ) );
    return outStatus;
}

MStatus plugin_manager::add_registry_item( detail::plugin_registry_item* item ) {
    MStatus status = item->init( *m_plugin );

    if( !status ) {
        status.perror( to_maya_t( _T("Error Initializing ") + item->description() ) );
        unregister_all();
        return status;
    } else {
        m_registeredItems.push_back( boost::shared_ptr<detail::plugin_registry_item>( item ) );
        return MStatus::kSuccess;
    }
}

template <typename T>
MStatus plugin_manager::register_callback( MSceneMessage::Message msg, typename T::function_t func, void* clientData ) {
    return add_registry_item( new detail::plugin_callback_item<T>( msg, func, clientData ) );
}

template MStatus
plugin_manager::register_callback<detail::register_callback_t>( MSceneMessage::Message,
                                                                detail::register_callback_t::function_t, void* );
template MStatus plugin_manager::register_callback<detail::register_check_callback_t>(
    MSceneMessage::Message, detail::register_check_callback_t::function_t, void* );
template MStatus plugin_manager::register_callback<detail::register_check_file_callback_t>(
    MSceneMessage::Message, detail::register_check_file_callback_t::function_t, void* );
template MStatus plugin_manager::register_callback<detail::register_string_array_callback_t>(
    MSceneMessage::Message, detail::register_string_array_callback_t::function_t, void* );

MStatus plugin_manager::register_command( const MString& commandName, MCreatorFunction creator,
                                          MCreateSyntaxFunction createSyntaxFunction ) {
    return add_registry_item( new detail::plugin_command_item( commandName, creator, createSyntaxFunction ) );
}

MStatus plugin_manager::register_data( const MString& typeName, const MTypeId& typeId, MCreatorFunction creatorFunction,
                                       MPxData::Type type ) {
    return add_registry_item( new detail::plugin_data_item( typeName, typeId, creatorFunction, type ) );
}

MStatus plugin_manager::register_mel_scripts( const frantic::tstring& initScript, const frantic::tstring& deinitScript,
                                              const frantic::tstring& description ) {
    return add_registry_item( new detail::plugin_mel_scripts_item( initScript, deinitScript, description ) );
}

MStatus plugin_manager::register_mel_script_files( const frantic::tstring& initScriptFile,
                                                   const frantic::tstring& deinitScriptFile,
                                                   const frantic::tstring& description ) {
    return add_registry_item( new detail::plugin_mel_scripts_item(
        make_mel_source_call( initScriptFile ), make_mel_source_call( deinitScriptFile ), description ) );
}

MStatus plugin_manager::register_python_scripts( const frantic::tstring& initScript,
                                                 const frantic::tstring& deinitScript,
                                                 const frantic::tstring& description ) {
    return add_registry_item( new detail::plugin_python_scripts_item( initScript, deinitScript, description ) );
}

MStatus plugin_manager::register_shape( const MString& nodeName, MTypeId typeId, MCreatorFunction nodeCreator,
                                        MInitializeFunction nodeInitializer, MCreatorFunction nodeUICreator,
                                        const MString* classification ) {
    return add_registry_item( new detail::plugin_shape_item( nodeName, typeId, nodeCreator, nodeInitializer,
                                                             nodeUICreator, classification ) );
}

MStatus plugin_manager::register_node( const MString& nodeName, MTypeId typeId, MCreatorFunction nodeCreator,
                                       MInitializeFunction nodeInitializer, MPxNode::Type nodeType,
                                       const MString* classification ) {
    return add_registry_item(
        new detail::plugin_node_item( nodeName, typeId, nodeCreator, nodeInitializer, nodeType, classification ) );
}

MStatus plugin_manager::register_ui( const MString& creationProc, const MString& deletionProc,
                                     const MString& creationBatchProc, const MString& deletionBatchProc ) {
    return add_registry_item(
        new detail::plugin_ui_item( creationProc, deletionProc, creationBatchProc, deletionBatchProc ) );
}

#if MAYA_API_VERSION >= 201200

MStatus
plugin_manager::register_geometry_override_creator( const MString& drawClassification, const MString& registrantId,
                                                    MHWRender::MDrawRegistry::GeometryOverrideCreator creator ) {
    return add_registry_item( new detail::plugin_geometry_override_item( drawClassification, registrantId, creator ) );
}

#endif

MStatus plugin_manager::unregister_all() {
    MStatus returnStatus = MStatus::kSuccess;

    for( plugin_registry_vector::reverse_iterator it = m_registeredItems.rbegin(); it != m_registeredItems.rend();
         ++it ) {
        MStatus status = ( *it )->deinit( *m_plugin );

        if( !status ) {
            status.perror( to_maya_t( _T("Error Unloading ") + ( *it )->description() ) );
            returnStatus = MStatus::kFailure;
        }
    }

    m_registeredItems.clear();

    return returnStatus;
}

bool plugin_manager::is_loaded() const { return m_plugin != NULL; }

frantic::tstring plugin_manager::get_plugin_path() const {
    return frantic::strings::to_tstring( m_plugin->loadPath().asChar() );
}

} // namespace maya
} // namespace frantic
