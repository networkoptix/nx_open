/*
 * pluginmanager.h
 *
 *  Created on: Sep 11, 2012
 *      Author: a.kolesnikov
 */

#ifndef PLUGINLOADER_H_
#define PLUGINLOADER_H_

#include <QObject>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPluginLoader>
#include <QSharedPointer>
#include <QString>

#include "nx_plugin_api.h"


//!Loads custom application plugins and provides plugin management methods
/*!
    Plugins are searched for in following directories:\n
    - plugin directory, supplied in constructor
    - on linux, directory, defined by environment variable NETWORK_OPTIX_PLUGIN_DIR, is searched
    - process current directory
    - process binary directory

    \note This class implements single-tone
    \note Methods of class are thread-safe
*/
class PluginManager
:
    public QObject
{
    Q_OBJECT

public:
    PluginManager( const QString& pluginDir = QString() );
    virtual ~PluginManager();

    //!Searches for plugins of type \a T among loaded plugins
    template<class T> QList<T*> findPlugins() const
    {
        QMutexLocker lk( &m_mutex );

        QList<T*> foundPlugins;
        foreach( QSharedPointer<QPluginLoader> plugin, m_loadedPlugins )
        {
            T* foundPlugin = qobject_cast<T*>(plugin->instance());
            if( foundPlugin )
                foundPlugins.push_back( foundPlugin );
        }

        return foundPlugins;
    }

    //!Searches for plugins of type \a T, derived from \a nxpl::NXPluginInterface among loaded plugins
    template<class T> QList<T*> findNXPlugins() const
    {
        foreach( nxpl::NXPluginInterface* plugin, m_nxPlugins )
        {
            void* ptr = plugin->queryInterface( T::INTF_GUID );
            if( ptr )
                foundPlugins.push_back( static_cast<T*>(ptr) );
        }

        return foundPlugins;
    }

    /*!
        This method must be called implicitly
    */
    void loadPlugins();

    //!Guess what
    static PluginManager* instance( const QString& pluginDir = QString() );

signals:
    //!Emitted just after new plugin has been loaded
    void pluginLoaded();

private:
    const QString m_pluginDir;
    QList<QSharedPointer<QPluginLoader> > m_loadedPlugins;
    QList<nxpl::NXPluginInterface*> m_nxPlugins;
    mutable QMutex m_mutex;

    void loadPlugins( const QString& dirToSearchIn );
    bool loadQtPlugin( const QString& fullFilePath );
    //!Loads \a nxpl::NXPluginInterface based plugin
    bool loadNXPlugin( const QString& fullFilePath );
};

#endif /* PLUGINLOADER_H_ */
