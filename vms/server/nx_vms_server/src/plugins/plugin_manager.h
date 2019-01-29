#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QPluginLoader>
#include <QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QSettings>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <nx/sdk/helpers/ptr.h>
#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>
#include <plugins/settings.h>

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
    PluginManager(QObject* parent);

    virtual ~PluginManager();

    /**
     * Searches for plugins that implement the specified interface derived from
     * nxpl::PluginInterface among loaded plugins.
     *
     * Increments (implicitly using queryInterface()) reference counter for the returned pointers.
     */
    template<class Interface>
    QList<Interface*> findNxPlugins(const nxpl::NX_GUID& guid) const
    {
        QList<Interface*> foundPlugins;
        for (const auto& plugin: m_nxPlugins)
        {
            if (const auto ptr = plugin->queryInterface(guid))
                foundPlugins.push_back(static_cast<Interface*>(ptr));
        }
        return foundPlugins;
    }

    /**
     * @param settings This settings are reported to each plugin (if supported by the plugin).
     */
    void loadPlugins(const QSettings* settings);

signals:
    /** Emitted just after new plugin has been loaded. */
    void pluginLoaded();

private:
    void loadPluginsFromDirWithBlackList(
        const QStringList& disabledLibNames,
        const nx::plugins::SettingsHolder& settingsHolder,
        const QString& dirToSearchIn);

    void loadPluginsFromDirWithWhiteList(
        const QStringList& enabledLibNames,
        const nx::plugins::SettingsHolder& settingsHolder,
        const QString& dirToSearchIn);

    /** Loads nxpl::PluginInterface-based plugin. */
    bool loadNxPlugin(
        const nx::plugins::SettingsHolder& settingsHolder,
        const QString& filename,
        const QString& libName);

private:
    const nx::sdk::Ptr<nxpl::PluginInterface> m_pluginContainer;
    QList<QSharedPointer<QPluginLoader>> m_qtPlugins;
    std::vector<nx::sdk::Ptr<nxpl::PluginInterface>> m_nxPlugins;
    mutable QnMutex m_mutex;
};
