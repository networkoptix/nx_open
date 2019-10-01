#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSettings>
#include <QtCore/QLibrary>

#include <nx/utils/thread/mutex.h>

#include <nx/sdk/i_plugin.h>
#include <nx/sdk/ptr.h>
#include <plugins/plugin_api.h>
#include <nx/vms/server/plugins/utility_provider.h>
#include <nx/vms/server/metrics/i_plugin_metrics_provider.h>
#include <plugins/settings.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/vms/server/server_module_aware.h>

class QDir;
class QFileInfo;

/**
 * Loads custom application plugins and provides plugin management methods. Plugins are looked for
 * in the hard-coded "plugins" directory located next to the executable, and additionally in the
 * directory defined by environment variable VMS_PLUGIN_DIR.
 *
 * NOTE: Methods of this class are thread-safe.
 */
class PluginManager:
    public QObject,
    public nx::vms::server::ServerModuleAware,
    public nx::vms::server::metrics::IPluginMetricsProvider
{
    Q_OBJECT

public:
    PluginManager(QnMediaServerModule* parent);

    virtual ~PluginManager();

    // TODO: Remove when Storage and Camera SDKs migrate into the new SDK.
    /**
     * Searches for plugins that implement the specified interface derived from
     * nxpl::PluginInterface (old SDK) among loaded plugins.
     *
     * Increments (implicitly using queryInterface()) reference counter for the returned pointers.
     */
    template<class Interface>
    QList<Interface*> findNxPlugins(const nxpl::NX_GUID& oldSdkInterfaceId) const
    {
        QList<Interface*> foundPlugins;
        for (const auto& pluginContext: m_pluginContexts)
        {
            auto plugin = pluginContext.plugin;
            if (!plugin)
                continue;

            // It is safe to treat new SDK plugins as if they were old SDK plugins for the purpose
            // of calling queryInterface(), hence querying all plugins - old and new.
            auto oldSdkPlugin = reinterpret_cast<nxpl::PluginInterface*>(plugin.get());
            if (const auto ptr = oldSdkPlugin->queryInterface(oldSdkInterfaceId))
                foundPlugins.push_back(static_cast<Interface*>(ptr));
        }
        return foundPlugins;
    }

    /**
     * Searches for plugins that implement the specified interface derived from IRefCountable,
     * among loaded plugins.
     *
     * Increments (implicitly using queryInterface()) reference counter for the returned pointers.
     */
    template<class Interface>
    QList<nx::sdk::Ptr<Interface>> findNxPlugins() const
    {
        QList<nx::sdk::Ptr<Interface>> foundPlugins;
        for (const auto& pluginContext: m_pluginContexts)
        {
            auto plugin = pluginContext.plugin;
            if (!plugin)
                continue;

            if (const auto ptr = plugin->queryInterface<Interface>())
                foundPlugins.push_back(ptr);
        }
        return foundPlugins;
    }

    /**
     * @param settings Mediaserver settings (aka "roSettings"). They are passed to the old SDK
     *     plugins which implement setSettings() method.
     */
    void loadPlugins(const QSettings* settings);

    void unloadPlugins();

    nx::vms::api::PluginInfoList pluginInfoList() const;

    virtual std::vector<nx::vms::server::metrics::PluginMetrics> metrics() const override;

    /** @return Never null; if not found, fails an assertion and returns a stub structure. */
    std::shared_ptr<const nx::vms::api::PluginInfo> pluginInfo(
        const nx::sdk::IPlugin* plugin) const;

    void setIsActive(const nx::sdk::IRefCountable* plugin, bool isActive);

signals:
    /** Emitted just after new plugin has been loaded. */
    void pluginLoaded();

private:
    using PluginInfo = nx::vms::api::PluginInfo;
    using PluginInfoPtr = std::shared_ptr<PluginInfo>;
    using SettingsHolder = nx::plugins::SettingsHolder;
    using Status = PluginInfo::Status;
    using Error = PluginInfo::Error;
    using Optionality = PluginInfo::Optionality;
    using MainInterface = PluginInfo::MainInterface;

    void storeInternalErrorPluginInfo(
        PluginInfoPtr pluginInfo,
        nx::sdk::Ptr<nx::sdk::IRefCountable> plugin,
        const QString& errorMessage);

    bool storeNotLoadedPluginInfo(
        PluginInfoPtr pluginInfo, Status status, Error errorCode, const QString& reason);

    bool storeLoadedPluginInfo(
        PluginInfoPtr pluginInfo, nx::sdk::Ptr<nx::sdk::IRefCountable> plugin);

    bool processPluginEntryPointForNewSdk(
        nx::sdk::IPlugin::EntryPointFunc entryPointFunc,
        PluginInfoPtr pluginInfo);

    bool processPluginEntryPointForOldSdk(
        nxpl::Plugin::EntryPointFunc entryPointFunc,
        const SettingsHolder& settingsHolder,
        PluginInfoPtr pluginInfo);

    bool processPluginLib(
        QLibrary* lib, const SettingsHolder& settingsHolder, PluginInfoPtr pluginInfo);

    void loadPlugin(
        const SettingsHolder& settingsHolder, PluginInfoPtr pluginInfo);

    void loadPluginsFromDir(
        const SettingsHolder& settingsHolder,
        const QDir& dirToSearch,
        Optionality pluginLoadingType,
        std::function<bool(PluginInfoPtr pluginInfo)> allowPlugin);

    void loadOptionalPluginsIfNeeded(
        const QString& binPath, const SettingsHolder& settingsHolder);

    void loadNonOptionalPluginsIfNeeded(
        const QString& binPath, const SettingsHolder& settingsHolder);

private:
    struct PluginContext
    {
        PluginInfoPtr pluginInfo;

        // For compatibility with the old-SDK-based plugins, we store pointers to the base interface
        // rather than nx::sdk::IPlugin.
        nx::sdk::Ptr<nx::sdk::IRefCountable> plugin;
    };

    std::vector<PluginContext> m_pluginContexts;
    mutable std::vector<PluginInfo> m_cachedPluginInfo;

    mutable QnMutex m_mutex;
};
