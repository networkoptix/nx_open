/*
 * pluginmanager.cpp
 *
 *  Created on: Sep 11, 2012
 *      Author: ak
 */

#include "pluginmanager.h"

#include <QAtomicPointer>
#include <QCoreApplication>
#include <QDir>

#include "../decoders/abstractclientplugin.h"
#include "../decoders/abstractvideodecoderplugin.h"
#include "../utils/common/log.h"


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
        delete m_pluginManager;
        m_pluginManager = NULL;
    }

    PluginManager* getPluginManager( const QString& pluginDir )
    {
        if( !m_pluginManager )
        {
            PluginManager* newInstance = new PluginManager( pluginDir );
            if( !m_pluginManager.testAndSetOrdered( NULL, newInstance ) )
                delete newInstance;
            //else
            //    newInstance->loadPlugins();
        }
        return m_pluginManager;
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
        it = m_loadedPlugins.begin();
        it != m_loadedPlugins.end();
        ++it )
    {
        emit pluginUnloaded();
        (*it)->unload();
    }
}

//!Guess what
PluginManager* PluginManager::instance( const QString& pluginDir )
{
    return pluginManagerWrapper()->getPluginManager( pluginDir );

    //if( !pluginManagerInstance )
    //{
    //    PluginManager* newInstance = new PluginManager( pluginDir );
    //    if( !pluginManagerInstance.testAndSetOrdered( NULL, newInstance ) )
    //        delete newInstance;
    //    //else
    //    //    newInstance->loadPlugins();
    //}
    //return pluginManagerInstance;
}

void PluginManager::loadPlugins()
{
    QMutexLocker lk( &m_mutex );

    //loading plugins
    if( !m_pluginDir.isEmpty() )
        loadPlugins( m_pluginDir );

#ifndef Q_OS_WIN32
    char* netOptixPluginDir = getenv("NETWORK_OPTIX_PLUGIN_DIR");
    if( netOptixPluginDir )
        loadPlugins( QString::fromAscii(netOptixPluginDir) );
#endif

    loadPlugins( QDir::currentPath() );
    loadPlugins( QCoreApplication::applicationDirPath() );
}

void PluginManager::loadPlugins( const QString& dirToSearchIn )
{
    QDir pluginDir( dirToSearchIn );
    const QStringList& entries = pluginDir.entryList( QStringList(), QDir::Files | QDir::Readable );
    foreach( const QString& entry, entries )
    {
        if( !QLibrary::isLibrary( entry ) )
            continue;
        loadPlugin( pluginDir.path() + QString::fromAscii("/") + entry );
    }
}

void PluginManager::loadPlugin( const QString& fullFilePath )
{
    QSharedPointer<QPluginLoader> plugin( new QPluginLoader( fullFilePath ) );
    if( !plugin->load() )
    {
        cl_log.log( QString::fromAscii("Library %1 is not plugin").arg(fullFilePath), cl_logDEBUG1 );
        return;
    }

    QObject* obj = plugin->instance();
    QnAbstractClientPlugin* clientPlugin = dynamic_cast<QnAbstractClientPlugin*>(obj);
    if( !clientPlugin )
        return;
    clientPlugin->initializeLog( QnLog::instance() );
    if( !clientPlugin->initialized() )
    {
        cl_log.log( QString::fromAscii("Failed to initialize plugin %1").arg(fullFilePath), cl_logERROR );
        return;
    }

    cl_log.log( QString::fromAscii("Successfully loaded plugin %1").arg(fullFilePath), cl_logWARNING );
    m_loadedPlugins.push_back( plugin );

    emit pluginLoaded();
}
