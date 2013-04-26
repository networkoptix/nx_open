/*
 * pluginmanager.cpp
 *
 *  Created on: Sep 11, 2012
 *      Author: ak
 */

#include "pluginmanager.h"

#include <set>

#include <QAtomicPointer>
#include <QCoreApplication>
#include <QDir>

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
    QMutexLocker lk( &m_mutex );

    std::set<QString> directoriesToSearchForPlugins;

    //loading plugins
    if( !m_pluginDir.isEmpty() )
        directoriesToSearchForPlugins.insert( QDir(m_pluginDir).absolutePath() );

#ifndef Q_OS_WIN32
    char* netOptixPluginDir = getenv("NETWORK_OPTIX_PLUGIN_DIR");
    if( netOptixPluginDir )
        directoriesToSearchForPlugins.insert( QString::fromLatin1(netOptixPluginDir) );
#endif

    //directoriesToSearchForPlugins.insert( QDir(QDir::currentPath()).absolutePath() );
    directoriesToSearchForPlugins.insert( QDir(QCoreApplication::applicationDirPath()).absolutePath() );

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
    foreach( const QString& entry, entries )
    {
        if( !QLibrary::isLibrary( entry ) )
            continue;

        if( pluginsToLoad & ptQt )
            loadQtPlugin( pluginDir.path() + QString::fromAscii("/") + entry );

        if( pluginsToLoad & ptNX )
            loadNxPlugin( pluginDir.path() + QString::fromAscii("/") + entry );
    }
}

bool PluginManager::loadQtPlugin( const QString& fullFilePath )
{
    QSharedPointer<QPluginLoader> plugin( new QPluginLoader( fullFilePath ) );
    if( !plugin->load() )
    {
        cl_log.log( QString::fromAscii("Library %1 is not plugin").arg(fullFilePath), cl_logDEBUG1 );
        return false;
    }

    QObject* obj = plugin->instance();
    QnAbstractClientPlugin* clientPlugin = dynamic_cast<QnAbstractClientPlugin*>(obj);
    if( !clientPlugin )
        return false;
    clientPlugin->initializeLog( QnLog::instance() );
    if( !clientPlugin->initialized() )
    {
        cl_log.log( QString::fromAscii("Failed to initialize plugin %1").arg(fullFilePath), cl_logERROR );
        return false;
    }

    cl_log.log( QString::fromAscii("Successfully loaded plugin %1").arg(fullFilePath), cl_logWARNING );
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
        return false;

    nxpl::PluginInterface* obj = entryProc();
    if( !obj )
        return false;

    cl_log.log( QString::fromAscii("Successfully loaded NX plugin %1").arg(fullFilePath), cl_logWARNING );
    m_nxPlugins.push_back( obj );

    emit pluginLoaded();
    return true;
}
