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

#include "../utils/common/log.h"


PluginManager::PluginManager( const QString& pluginDir )
:
    m_pluginDir( pluginDir )
{
}

PluginManager::~PluginManager()
{
}

static QAtomicPointer<PluginManager> pluginManagerInstance;

//!Guess what
PluginManager* PluginManager::instance( const QString& pluginDir )
{
    if( !pluginManagerInstance )
    {
        PluginManager* newInstance = new PluginManager( pluginDir );
        if( pluginManagerInstance.testAndSetOrdered( NULL, newInstance ) )
            newInstance->loadPlugins();
        else
            delete newInstance;
    }
    return pluginManagerInstance;
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
        cl_log.log( QString::fromAscii("Library %1 is not plugin").arg(fullFilePath), cl_logWARNING );
        return;
    }

    cl_log.log( QString::fromAscii("Successfully loaded plugin %1").arg(fullFilePath), cl_logWARNING );
    m_loadedPlugins.push_back( plugin );
}
