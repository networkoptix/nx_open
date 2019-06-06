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
#include <nx/vms/server/plugins/utility_provider.h>
#include <plugins/settings.h>

#include <nx/vms/api/data/analytics_data.h>

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
        for (const auto& pluginContext: m_nxPlugins)
        {
            auto plugin = pluginContext.plugin;
            if (!plugin)
                continue;

            // It is safe to treat new SDK plugins as if they were old SDK plugins for the purpose
            // of calling queryInterface().
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
        for (const auto& pluginContext: m_nxPlugins)
        {
            auto plugin = pluginContext.plugin;
            if (!plugin)
                continue;

            if (const auto ptr = nx::sdk::queryInterfacePtr<Interface>(plugin))
                foundPlugins.push_back(ptr);
        }
        return foundPlugins;
    }

    /**
     * @param settings This settings are reported to each plugin (if supported by the plugin).
     */
    void loadPlugins(const QSettings* settings);


    void unloadPlugins();

    nx::vms::api::PluginInfoList pluginInformation() const;
signals:
    /** Emitted just after new plugin has been loaded. */
    void pluginLoaded();

private:
    void loadNonOptionalPluginsIfNeeded(
        const QString& binPath, const nx::plugins::SettingsHolder& settingsHolder);

    void loadOptionalPluginsIfNeeded(
        const QString& binPath, const nx::plugins::SettingsHolder& settingsHolder);

    void loadPluginsFromDir(
        const nx::plugins::SettingsHolder& settingsHolder,
        const QDir& dirToSearchIn,
        nx::vms::api::PluginInfo::Optionality pluginLoadingType,
        std::function<bool(const QFileInfo& pluginFileInfo)> allowPlugin);

    bool loadNxPlugin(
        const nx::plugins::SettingsHolder& settingsHolder,
        const QString& linkedLibsDirectory,
        const QString& libFilename,
        const QString& libName,
        nx::vms::api::PluginInfo pluginInfo);

    std::unique_ptr<QLibrary> loadPluginLibrary(
        const QString& linkedLibsDir, const QString& libFilename);

    void storeInfoAboutLoadingError(
        nx::vms::api::PluginInfo,
        nx::vms::api::PluginInfo::Status loadingStatus,
        nx::vms::api::PluginInfo::Error errorCode,
        QString statusMessage);

private:
    const nx::sdk::Ptr<nx::sdk::IUtilityProvider> m_utilityProvider;


    struct PluginContext
    {
        nx::vms::api::PluginInfo pluginInformation;

        // For compatibility with the old-SDK-based plugins, we store pointers to the base interface
        // rather than nx::sdk::IPlugin.
        nx::sdk::Ptr<nx::sdk::IRefCountable> plugin;
    };

    std::vector<PluginContext> m_nxPlugins;
    mutable std::vector<nx::vms::api::PluginInfo> m_cachedPluginInfo;

    mutable QnMutex m_mutex;
};
