#include "plugin_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>
#include <nx/kit/utils.h>

#include <plugins/plugin_api.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/vms/server/plugins/utility_provider.h>
#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/api/analytics/plugin_manifest.h>

#include "vms_server_plugins_ini.h"

using namespace nx::sdk;

static QStringList stringToListViaComma(const QString& s)
{
    QStringList list = s.split(",");
    for (auto& item: list)
        item = item.trimmed();
    return list;
}

PluginManager::PluginManager(QObject* parent):
    QObject(parent),
    m_utilityProvider(new nx::vms::server::plugins::UtilityProvider())
{
}

PluginManager::~PluginManager()
{
}

void PluginManager::unloadPlugins()
{
    m_nxPlugins.clear();
}

/**
 * @return Empty string if the file is not a library.
 */
static QString libNameFromFileInfo(const QFileInfo& fileInfo)
{
    if (!QLibrary::isLibrary(fileInfo.absoluteFilePath()))
        return "";

    QString libName = fileInfo.baseName().left(fileInfo.baseName().lastIndexOf("."));
    static const QString kPrefix{"lib"};
    if (libName.startsWith(kPrefix))
        libName.remove(0, kPrefix.size());

    return libName;
}

static QFileInfoList pluginFileInfoList(const QDir& dirToSearchIn, bool searchInnerDirs = true)
{
    QDir::Filters filters = QDir::Files | QDir::Readable;
    QFileInfoList filteredEntries;

    if (searchInnerDirs)
        filters |= QDir::Dirs | QDir::NoDotAndDotDot;

    const auto entries = dirToSearchIn.entryInfoList(/*nameFilters*/ QStringList(), filters);

    for (const auto& entry: entries)
    {
        if (entry.isDir())
        {
            if (pluginsIni().tryAllLibsInPluginDir)
            {
                filteredEntries << pluginFileInfoList(
                    entry.absoluteFilePath(),
                    /*searchInnerDirs*/ false);
            }
            else
            {
                const QDir entryAsDir = entry.absoluteFilePath();
                const auto pluginDirEntries = entryAsDir.entryInfoList(
                    /*nameFilters*/ QStringList(), QDir::Files | QDir::Readable);

                for (const auto& pluginDirEntry: pluginDirEntries)
                {
                    if (libNameFromFileInfo(pluginDirEntry) == entry.fileName())
                    {
                        filteredEntries << pluginDirEntry;
                        break;
                    }
                }
            }
        }
        else //< entry is a file.
        {
            if (!libNameFromFileInfo(entry).isEmpty())
                filteredEntries << entry;  //< It is a plugin entry.
        }
    }
    return filteredEntries;
}

void PluginManager::loadPluginsFromDir(
    const nx::plugins::SettingsHolder& settingsHolder,
    const QDir& dirToSearchIn,
    nx::vms::api::PluginModuleData::LoadingType pluginLoadingType,
    std::function<bool(const QFileInfo& pluginFileInfo)> allowPlugin)
{
    using nx::vms::api::PluginModuleData;
    for (const auto& fileInfo: pluginFileInfoList(dirToSearchIn))
    {
        const QString& libName = libNameFromFileInfo(fileInfo);

        PluginModuleData pluginInfo;
        pluginInfo.loadingType = pluginLoadingType;
        pluginInfo.libraryName = fileInfo.absoluteFilePath();

        if (!allowPlugin(fileInfo))
        {
            pluginInfo.status = (pluginLoadingType == PluginModuleData::LoadingType::normal)
                ? PluginModuleData::Status::notLoadedBecauseOfBlackList
                : PluginModuleData::Status::notLoadedBecausePluginIsOptional;

            pluginInfo.statusMessage = (pluginLoadingType == PluginModuleData::LoadingType::normal)
                ? "Plugin is in the black list"
                : "Optional plugin is not loaded because it is not in the white list";

            m_nxPlugins.push_back({std::move(pluginInfo), nullptr});
            continue;
        }

        // If the plugin is located in its dedicated directory, then use this directory for plugin
        // dependencies lookup, otherwise do not perform the dependencies lookup.
        const QString linkedLibsDirectory =
            (fileInfo.dir() != dirToSearchIn) // The plugin resides in its dedicated dir.
                ? fileInfo.dir().absolutePath()
                : "";

        loadNxPlugin(
            settingsHolder,
            linkedLibsDirectory,
            fileInfo.absoluteFilePath(),
            libName,
            std::move(pluginInfo));
        // Ignore return value - an error is already logged.
    }
}

void PluginManager::loadPlugins(const QSettings* settings)
{
    QnMutexLocker lock(&m_mutex);

    const nx::plugins::SettingsHolder settingsHolder{settings};
    if (!NX_ASSERT(settingsHolder.isValid()))
        return;

    const QString binPath = QDir(QCoreApplication::applicationDirPath()).absolutePath();

    loadNonOptionalPluginsIfNeeded(binPath, settingsHolder);
    loadOptionalPluginsIfNeeded(binPath, settingsHolder);
}

void PluginManager::loadNonOptionalPluginsIfNeeded(
    const QString& binPath, const nx::plugins::SettingsHolder& settingsHolder)
{
    const QString disabledNxPlugins =
        QString::fromLatin1(pluginsIni().disabledNxPlugins).trimmed();

    if (disabledNxPlugins != "*")
    {
        std::set<QString> directoriesToSearchForPlugins;

        const char* const vmsPluginDir = getenv("VMS_PLUGIN_DIR");
        if (vmsPluginDir)
            directoriesToSearchForPlugins.insert(QString::fromLatin1(vmsPluginDir));

        directoriesToSearchForPlugins.insert(binPath + "/plugins/");

        const QStringList disabledLibNames = stringToListViaComma(disabledNxPlugins);
        for (const QString& dir: directoriesToSearchForPlugins)
        {
            loadPluginsFromDir(
                settingsHolder,
                dir,
                nx::vms::api::PluginModuleData::LoadingType::normal,
                [this, disabledLibNames](const QFileInfo& pluginFileInfo)
                {
                    if (!disabledLibNames.contains(libNameFromFileInfo(pluginFileInfo)))
                        return true;
                    NX_INFO(this, "Skipped loading Nx plugin [%1] (blacklisted by %2)",
                        pluginFileInfo.absoluteFilePath(), pluginsIni().iniFile());
                    return false;
                });
        }
    }
    else
    {
        NX_INFO(this, "Skipped loading all non-optional Nx plugins (as per %1)",
            pluginsIni().iniFile());
    }
}

void PluginManager::loadOptionalPluginsIfNeeded(
    const QString& binPath, const nx::plugins::SettingsHolder& settingsHolder)
{
    // Load optional plugins, if demanded by .ini.
    const QString enabledNxPluginsOptional =
        QString::fromLatin1(pluginsIni().enabledNxPluginsOptional).trimmed();
    if (!enabledNxPluginsOptional.isEmpty())
    {
        NX_INFO(this, "Loading optional Nx plugins, if any (as per %1)",
            pluginsIni().iniFile());

        const QString optionalPluginsDir = binPath + "/plugins_optional/";
        const QStringList enabledLibNames = (enabledNxPluginsOptional == "*")
            ? QStringList()
            : stringToListViaComma(enabledNxPluginsOptional);

        loadPluginsFromDir(
            settingsHolder,
            optionalPluginsDir,
            nx::vms::api::PluginModuleData::LoadingType::optional,
            [this, enabledLibNames](const QFileInfo& pluginFileInfo)
            {
                if (enabledLibNames.isEmpty()
                    || enabledLibNames.contains(libNameFromFileInfo(pluginFileInfo)))
                {
                    return true;
                }
                NX_INFO(this, "Skipped loading Nx plugin [%1] (not whitelisted by %2)",
                    pluginFileInfo.absoluteFilePath(), pluginsIni().iniFile());
                return false;
            });
    }
}

/**
 * @param linkedLibsDirectory Can be empty if the plugin is not contained in its dedicated dir.
 */
std::unique_ptr<QLibrary> PluginManager::loadPluginLibrary(
    const QString& linkedLibsDirectory,
    const QString& libFilename)
{
    #if defined(_WIN32)
        if (!pluginsIni().disablePluginLinkedDllLookup && !linkedLibsDirectory.isEmpty())
        {
            NX_DEBUG(this, "Calling SetDllDirectoryW(%1)",
                nx::kit::utils::toString(linkedLibsDirectory));
            if (!SetDllDirectoryW(linkedLibsDirectory.toStdWString().c_str()))
            {
                NX_ERROR(this, "SetDllDirectoryW(%1) failed",
                    nx::kit::utils::toString(linkedLibsDirectory));
            }
        }
    #endif

    auto lib = std::make_unique<QLibrary>(libFilename);

    // Flag DeepBindHint forces plugin (the loaded side) to use its functions instead of the same
    // named functions of the Server (the loading side). In Linux it is not so by default.
    QLibrary::LoadHints hints = lib->loadHints();
    hints |= QLibrary::DeepBindHint;
    lib->setLoadHints(hints);

    const bool libLoaded = lib->load();

    #if defined(_WIN32)
        if (!pluginsIni().disablePluginLinkedDllLookup && !linkedLibsDirectory.isEmpty())
        {
            NX_DEBUG(this, "Calling SetDllDirectoryW(nullptr)");
            if (!SetDllDirectoryW(nullptr))
                NX_ERROR(this, "SetDllDirectoryW(nullptr) failed");
        }
    #endif

    if (!libLoaded)
    {
        NX_ERROR(this, "Failed to load Nx plugin [%1]: %2", libFilename, lib->errorString());
        return nullptr;
    }

    return lib;
}

void PluginManager::storeInfoAboutLoadingError(
    nx::vms::api::PluginModuleData pluginInfo,
    nx::vms::api::PluginModuleData::Status loadingStatus,
    QString statusMessage)
{
    pluginInfo.status = loadingStatus;
    pluginInfo.statusMessage = std::move(statusMessage);
    m_nxPlugins.push_back({std::move(pluginInfo), nullptr});
}

bool PluginManager::loadNxPlugin(
    const nx::plugins::SettingsHolder& settingsHolder,
    const QString& linkedLibsDirectory,
    const QString& libFilename,
    const QString& libName,
    nx::vms::api::PluginModuleData pluginInfo)
{
    using namespace nx::sdk;
    using namespace nx::vms::api;
    using namespace nx::vms::server;
    using nx::vms::api::analytics::PluginManifest;

    const auto lib = loadPluginLibrary(linkedLibsDirectory, libFilename);
    if (!lib)
        return false;

    if (const auto entryPointFunc = reinterpret_cast<nxpl::Plugin::EntryPointFunc>(
        lib->resolve(nxpl::Plugin::kEntryPointFuncName)))
    {
        // Old entry point found: currently, this is a Storage or Camera plugin.
        const auto plugin = entryPointFunc();
        if (!plugin)
        {
            storeInfoAboutLoadingError(
                std::move(pluginInfo),
                PluginModuleData::Status::notLoadedBecauseOfError,
                "Failed to load Nx old SDK plugin: entry function returned null");

            NX_ERROR(this, "Failed to load Nx old SDK plugin [%1]: entry function returned null",
                libFilename);
            lib->unload();
            return false;
        }

        static const QString kOldPluginLoadedMessagePrefix = "Loaded Nx old SDK plugin. Supported interface: ";

        pluginInfo.status = PluginModuleData::Status::loadedNormally;
        pluginInfo.statusMessage = kOldPluginLoadedMessagePrefix + "nxpl::PluginInterface";
        NX_INFO(this, "Loaded Nx old SDK plugin [%1]", libFilename);

        if (const auto plugin1 = queryInterfacePtr<nxpl::Plugin>(plugin, nxpl::IID_Plugin))
        {
            pluginInfo.name = plugin1->name();
            pluginInfo.statusMessage += pluginInfo.statusMessage = kOldPluginLoadedMessagePrefix + "nxpl::Plugin";

            // Pass Mediaserver settings (aka "roSettings") to the plugin.
            if (!settingsHolder.isEmpty())
                plugin1->setSettings(settingsHolder.array(), settingsHolder.size());
        }

        if (const auto plugin2 = queryInterfacePtr<nxpl::Plugin2>(plugin, nxpl::IID_Plugin2))
        {
            pluginInfo.name = plugin2->name();
            pluginInfo.statusMessage = kOldPluginLoadedMessagePrefix + "nxpl::Plugin2";
            plugin2->setPluginContainer(
                reinterpret_cast<nxpl::PluginInterface*>(m_utilityProvider.get()));
        }

        m_nxPlugins.push_back({
            std::move(pluginInfo),
            Ptr<IRefCountable>(reinterpret_cast<IRefCountable*>(plugin))});
    }
    else if (const auto entryPointFunc = reinterpret_cast<IPlugin::EntryPointFunc>(
        lib->resolve(IPlugin::kEntryPointFuncName)))
    {
        // New entry point found: currently, this is an Analytics plugin.

        const auto plugin = toPtr(entryPointFunc());
        if (!plugin)
        {
            storeInfoAboutLoadingError(
                std::move(pluginInfo),
                PluginModuleData::Status::notLoadedBecauseOfError,
                "Failed to load Nx plugin: entry function returned null");

            NX_ERROR(this, "Failed to load Nx plugin [%1]: entry function returned null",
                libFilename);
            lib->unload();
            return false;
        }

        pluginInfo.name = plugin->name();
        pluginInfo.status = PluginModuleData::Status::loadedNormally;
        pluginInfo.statusMessage = "Plugin has been successfully loaded";

        NX_INFO(this, "Loaded Nx plugin [%1]", libFilename);

        if (const auto analyticsPlugin = queryInterfacePtr<nx::sdk::analytics::IPlugin>(plugin))
        {
            if (plugin->name() != libName)
            {
                NX_INFO(this, "Analytics plugin name [%1] does not equal library name [%2]",
                    plugin->name(), libName);
            }

            const auto manifest = sdk_support::manifestFromSdkObject<PluginManifest>(analyticsPlugin);
            if (manifest)
            {
                pluginInfo.name = manifest->name;

                #if 0 //< TODO: uncomment this section when correspondent fields are in the manifest schema.
                    pluginInfo.description = manifest->description;
                    pluginInfo.version = manifest->version;
                    pluginInfo.vendor = manifest->vendor;
                #endif
            }
        }

        plugin->setUtilityProvider(m_utilityProvider.get());
        m_nxPlugins.push_back({std::move(pluginInfo), Ptr<IRefCountable>(plugin)});
    }
    else
    {

        storeInfoAboutLoadingError(
            std::move(pluginInfo),
            PluginModuleData::Status::notLoadedBecauseOfError,
            lm("Failed to load Nx plugin: Neither %1(), nor the old SDK's %2() functions found")
                .args(IPlugin::kEntryPointFuncName, nxpl::Plugin::kEntryPointFuncName));

        NX_ERROR(this, "Failed to load Nx plugin [%1]: "
            "Neither %2(), nor the old SDK's %3() functions found",
            libFilename, IPlugin::kEntryPointFuncName, nxpl::Plugin::kEntryPointFuncName);
        lib->unload();
        return false;
    }

    emit pluginLoaded();
    return true;
}

nx::vms::api::PluginModuleDataList PluginManager::pluginInformation() const
{
    if (m_cachedPluginInfo.empty())
    {
        for (const auto& pluginContext : m_nxPlugins)
            m_cachedPluginInfo.push_back(pluginContext.pluginInformation);
    }

    return m_cachedPluginInfo;
}
