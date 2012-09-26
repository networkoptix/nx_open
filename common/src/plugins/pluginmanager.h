/*
 * pluginmanager.h
 *
 *  Created on: Sep 11, 2012
 *      Author: a.kolesnikov
 */

#ifndef PLUGINLOADER_H_
#define PLUGINLOADER_H_

#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPluginLoader>
#include <QSharedPointer>
#include <QString>


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
{
public:
    //!Searches for plugins of type \a T among loaded plugins
    template<class T>
        QList<T*> findPlugins() const
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

    //!Guess what
    static PluginManager* instance( const QString& pluginDir = QString() );

private:
    const QString m_pluginDir;
    QList<QSharedPointer<QPluginLoader> > m_loadedPlugins;
    mutable QMutex m_mutex;

    PluginManager( const QString& pluginDir = QString() );
    ~PluginManager();

    void loadPlugins();
    void loadPlugins( const QString& dirToSearchIn );
    void loadPlugin( const QString& fullFilePath );
};

#endif /* PLUGINLOADER_H_ */
