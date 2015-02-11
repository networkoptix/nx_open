/*
 * plugin_manager.h
 *
 *  Created on: Sep 11, 2012
 *      Author: a.kolesnikov
 */

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <utils/common/mutex.h>
#include <utils/common/mutex.h>
#include <QtCore/QPluginLoader>
#include <QSharedPointer>
#include <QtCore/QString>

#include "plugin_api.h"


//!Loads custom application plugins and provides plugin management methods
/*!
    Plugins are searched for in following directories:\n
    - plugin directory, supplied in constructor
    - on linux, directory, defined by environment variable VMS_PLUGIN_DIR, is searched
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
    enum PluginType
    {
        QtPlugin = 1, //!< Qt-based plugins.
        NxPlugin = 2, //!< Plugins, implementing plugin_api.h.
        
        AllPlugins = QtPlugin | NxPlugin
    };

    PluginManager( const QString& pluginDir = QString() );
    virtual ~PluginManager();

    //!Searches for plugins of type \a T among loaded plugins
    template<class T> QList<T*> findPlugins() const
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );

        QList<T*> foundPlugins;
        for( QSharedPointer<QPluginLoader> plugin: m_qtPlugins )
        {
            T* foundPlugin = qobject_cast<T*>(plugin->instance());
            if( foundPlugin )
                foundPlugins.push_back( foundPlugin );
        }

        return foundPlugins;
    }

    //!Searches for plugins of type \a T, derived from \a nxpl::PluginInterface among loaded plugins
    /*!
        Increments (implicitly using queryInterface) reference counter for returned pointers
    */
    template<class T> QList<T*> findNxPlugins( const nxpl::NX_GUID& guid ) const
    {
        QList<T*> foundPlugins;
        for( nxpl::PluginInterface* plugin: m_nxPlugins )
        {
            void* ptr = plugin->queryInterface( guid );
            if( ptr )
                foundPlugins.push_back( static_cast<T*>(ptr) );
        }

        return foundPlugins;
    }

    /*!
        This method must be called implicitly
    */
    void loadPlugins( PluginType pluginsToLoad = AllPlugins );

    //!Guess what
    static PluginManager* instance( const QString& pluginDir = QString() );

signals:
    //!Emitted just after new plugin has been loaded
    void pluginLoaded();

private:
    const QString m_pluginDir;
    QList<QSharedPointer<QPluginLoader> > m_qtPlugins;
    QList<nxpl::PluginInterface*> m_nxPlugins;
    mutable QnMutex m_mutex;

    void loadPluginsFromDir( const QString& dirToSearchIn, PluginType pluginsToLoad );
    bool loadQtPlugin( const QString& fullFilePath );
    //!Loads \a nxpl::PluginInterface based plugin
    bool loadNxPlugin( const QString& fullFilePath );
};

#endif /* PLUGIN_MANAGER_H */
