#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QPluginLoader>
#include <QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QSettings>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>


//!Loads custom application plugins and provides plugin management methods
/*!
    Plugins are searched for in following directories:\n
    - plugin directory, supplied in constructor
    - on linux, directory, defined by environment variable VMS_PLUGIN_DIR, is searched
    - process current directory
    - process binary directory

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
        QtPlugin = 1, //< Qt-based plugins.
        NxPlugin = 2, //< Plugins, implementing plugin_api.h.

        AllPlugins = QtPlugin | NxPlugin
    };

    PluginManager(
        QObject* parent,
        const QString& pluginDir = QString(),
        nxpl::PluginInterface* const pluginContainer = nullptr);

    virtual ~PluginManager();

    //!Searches for plugins of type \a T among loaded plugins
    template<class T> QList<T*> findPlugins() const
    {
        QnMutexLocker lk( &m_mutex );

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
        \param settings This settings are reported to each plugin (if supported by plugin of course)
        This method must be called implicitly
    */
    void loadPlugins(
        const QSettings* settings,
        PluginType pluginsToLoad = AllPlugins );

signals:
    //!Emitted just after new plugin has been loaded
    void pluginLoaded();

private:
    const QString m_pluginDir;
    nxpl::PluginInterface* const m_pluginContainer;
    QList<QSharedPointer<QPluginLoader> > m_qtPlugins;
    QList<nxpl::PluginInterface*> m_nxPlugins;
    mutable QnMutex m_mutex;

    void loadPluginsFromDir(
        const std::vector<nxpl::Setting>& settingsForPlugin,
        const QString& dirToSearchIn,
        PluginType pluginsToLoad );
    bool loadQtPlugin( const QString& fullFilePath );
    //!Loads \a nxpl::PluginInterface based plugin
    bool loadNxPlugin(
        const std::vector<nxpl::Setting>& settingsForPlugin,
        const QString& fileDir,
        const QString& fileName);
};
