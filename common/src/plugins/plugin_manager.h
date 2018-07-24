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
#include <nx/sdk/metadata/plugin.h>

/**
 * Loads custom application plugins and provides plugin management methods. Plugins are looked for
 * in the hard-coded "plugins" directory located next to the executable, and additionally in the
 * directory defined by environment variable VMS_PLUGIN_DIR.
 *
 * NOTE: Methods of this class are thread-safe.
 */
class PluginManager: public QObject
{
    Q_OBJECT

public:
    PluginManager(QObject* parent = nullptr, nxpl::PluginInterface* pluginContainer = nullptr);

    virtual ~PluginManager();

    /** Searches for plugins of type T among loaded plugins. */
    template<class T>
    QList<T*> findPlugins() const
    {
        QnMutexLocker lock(&m_mutex);

        QList<T*> foundPlugins;
        for (QSharedPointer<QPluginLoader> plugin: m_qtPlugins)
        {
            T* foundPlugin = qobject_cast<T*>(plugin->instance());
            if (foundPlugin)
                foundPlugins.push_back(foundPlugin);
        }

        return foundPlugins;
    }

    /**
     * Searches for plugins of type T, derived from nxpl::PluginInterface among loaded plugins.
     *
     * Increments (implicitly using queryInterface()) reference counter for the returned pointers.
     */
    template<class T>
    QList<T*> findNxPlugins(const nxpl::NX_GUID& guid) const
    {
        QList<T*> foundPlugins;
        for (nxpl::PluginInterface* plugin: m_nxPlugins)
        {
            void* ptr = plugin->queryInterface(guid);
            if (ptr)
                foundPlugins.push_back(static_cast<T*>(ptr));
        }

        return foundPlugins;
    }

    /**
     * @return Name of the plugin dynamic library, without "lib" prefix and without extension, or
     * null string if not found.
     */
    QString pluginLibName(const nxpl::PluginInterface* plugin)
    {
        return m_libNameByNxPlugin.value(plugin);
    }

    /**
     * @param settings This settings are reported to each plugin (if supported by the plugin, of 
     * course).
     */
    void loadPlugins(const QSettings* settings);

signals:
    /** Emitted just after new plugin has been loaded. */
    void pluginLoaded();

private:
    void loadPluginsFromDirWithBlackList(
        const QStringList& disabledLibNames,
        const std::vector<nxpl::Setting>& settingsForPlugin,
        const QString& dirToSearchIn);

    void loadPluginsFromDirWithWhiteList(
        const QStringList& enabledLibNames,
        const std::vector<nxpl::Setting>& settingsForPlugin,
        const QString& dirToSearchIn);

    /** Loads nxpl::PluginInterface-based plugin */
    bool loadNxPlugin(
        const std::vector<nxpl::Setting>& settingsForPlugin,
        const QString& filename,
        const QString& libName);

private:
    nxpl::PluginInterface* const m_pluginContainer;
    QList<QSharedPointer<QPluginLoader>> m_qtPlugins;
    QList<nxpl::PluginInterface*> m_nxPlugins;
    QHash<const nxpl::PluginInterface*, QString> m_libNameByNxPlugin;
    mutable QnMutex m_mutex;
};
