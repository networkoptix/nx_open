/*
 * pluginmanager.cpp
 *
 *  Created on: Sep 11, 2012
 *      Author: ak
 */

#include "plugin_manager.h"

#include <algorithm>
#include <set>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include "../decoders/abstractclientplugin.h"
#include "../decoders/abstractvideodecoderplugin.h"
#include "../utils/common/log.h"
#include "camera_plugin.h"


class PluginManagerWrapper
{
public:
    PluginManagerWrapper()
    :
        m_pluginManager( NULL )
    {
    }

    ~PluginManagerWrapper()
    {
        delete m_pluginManager.load();
        m_pluginManager.store(NULL);
    }

    PluginManager* getPluginManager( const QString& pluginDir )
    {
        if( !m_pluginManager.load() )
        {
            PluginManager* newInstance = new PluginManager( pluginDir );
            if( !m_pluginManager.testAndSetOrdered( NULL, newInstance ) )
                delete newInstance;
            //else
            //    newInstance->loadPlugins();
        }
        return m_pluginManager.load();
    }

private:
    QAtomicPointer<PluginManager> m_pluginManager;
};


Q_GLOBAL_STATIC(PluginManagerWrapper, pluginManagerWrapper);

//static QAtomicPointer<PluginManager> pluginManagerInstance;

PluginManager::PluginManager( const QString& pluginDir )
:
    m_pluginDir( pluginDir )
{
}

PluginManager::~PluginManager()
{
    for( QList<QSharedPointer<QPluginLoader> >::iterator
        it = m_qtPlugins.begin();
        it != m_qtPlugins.end();
        ++it )
    {
        (*it)->unload();
    }

    //releasing plugins
    std::for_each( m_nxPlugins.begin(), m_nxPlugins.end(), std::mem_fun( &nxpl::PluginInterface::releaseRef ) );
}

//!Guess what
PluginManager* PluginManager::instance( const QString& pluginDir )
{
    return pluginManagerWrapper()->getPluginManager( pluginDir );
}

void PluginManager::loadPlugins( PluginManager::PluginType pluginsToLoad )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    std::set<QString> directoriesToSearchForPlugins;

    //loading plugins
    if( !m_pluginDir.isEmpty() )
        directoriesToSearchForPlugins.insert( QDir(m_pluginDir).absolutePath() );

#ifndef Q_OS_WIN32
    char* vmsPluginDir = getenv("VMS_PLUGIN_DIR");
    if( vmsPluginDir )
        directoriesToSearchForPlugins.insert( QString::fromLatin1(vmsPluginDir) );
#endif

    //directoriesToSearchForPlugins.insert( QDir(QDir::currentPath()).absolutePath() );
    directoriesToSearchForPlugins.insert( QDir(QCoreApplication::applicationDirPath()).absolutePath() + lit("/plugins/") );

    for( std::set<QString>::const_iterator
        it = directoriesToSearchForPlugins.begin();
        it != directoriesToSearchForPlugins.end();
        ++it )
    {
        loadPluginsFromDir( *it, pluginsToLoad );
    }

    //loadPluginsFromDir( QCoreApplication::applicationDirPath(), pluginsToLoad );
}

void PluginManager::loadPluginsFromDir( const QString& dirToSearchIn, PluginType pluginsToLoad )
{
    QDir pluginDir( dirToSearchIn );
    const QStringList& entries = pluginDir.entryList( QStringList(), QDir::Files | QDir::Readable );
    for( const QString& entry: entries )
    {
        if( !QLibrary::isLibrary( entry ) )
            continue;

        if( pluginsToLoad & QtPlugin )
            loadQtPlugin( pluginDir.path() + lit("/") + entry );

        if( pluginsToLoad & NxPlugin )
            loadNxPlugin( pluginDir.path() + lit("/") + entry );
    }
}

bool PluginManager::loadQtPlugin( const QString& fullFilePath )
{
    QSharedPointer<QPluginLoader> plugin( new QPluginLoader( fullFilePath ) );
    if( !plugin->load() )
        return false;

    QObject* obj = plugin->instance();
    QnAbstractClientPlugin* clientPlugin = dynamic_cast<QnAbstractClientPlugin*>(obj);
    if( !clientPlugin )
        return false;
    clientPlugin->initializeLog( QnLog::instance() );
    if( !clientPlugin->initialized() )
    {
        NX_LOG( lit("Failed to initialize Qt plugin %1").arg(fullFilePath), cl_logERROR );
        return false;
    }

    NX_LOG( lit("Successfully loaded Qt plugin %1").arg(fullFilePath), cl_logWARNING );
    m_qtPlugins.push_back( plugin );

    emit pluginLoaded();
    return true;
}

bool PluginManager::loadNxPlugin( const QString& fullFilePath )
{
    QLibrary lib( fullFilePath );
    if( !lib.load() )
        return false;
    
    nxpl::CreateNXPluginInstanceProc entryProc = (nxpl::CreateNXPluginInstanceProc)lib.resolve( "createNXPluginInstance" );
    if( entryProc == NULL )
    {
        lib.unload();
        return false;
    }

    nxpl::PluginInterface* obj = entryProc();
    if( !obj )
    {
        lib.unload();
        return false;
    }

    NX_LOG( lit("Successfully loaded NX plugin %1").arg(fullFilePath), cl_logWARNING );
    m_nxPlugins.push_back( obj );

    emit pluginLoaded();
    return true;
}
