#include "plugin_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <nx/utils/log/log.h>
#include <nx/kit/utils.h>

#include <plugins/plugin_api.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/vms/server/plugins/utility_provider.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/sdk_support/to_string.h>
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

PluginManager::PluginManager(QObject* parent): QObject(parent)
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
    PluginInfo::Optionality optionality,
    std::function<bool(const QFileInfo& pluginFileInfo, PluginInfoPtr pluginInfo)> allowPlugin)
{
    for (const auto& fileInfo: pluginFileInfoList(dirToSearchIn))
    {
        const QString& libName = libNameFromFileInfo(fileInfo);

        PluginInfoPtr pluginInfo = std::make_shared<PluginInfo>();
        pluginInfo->optionality = optionality;
        pluginInfo->libraryFilename = fileInfo.absoluteFilePath();

        // If the plugin is located in its dedicated directory, then set this directory as its home
        // directory, otherwise, set empty home directory. If a plugin has a home directory, it is
        // used for its dynamic library dependencies lookup.
        pluginInfo->homeDir =
            (fileInfo.dir() != dirToSearchIn) // The plugin resides in its dedicated dir.
                ? fileInfo.dir().absolutePath()
                : "";

        if (!allowPlugin(fileInfo, pluginInfo))
            continue;

        loadNxPlugin(
            settingsHolder,
            pluginInfo->homeDir,
            fileInfo.absoluteFilePath(),
            libName,
            pluginInfo);
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
                PluginInfo::Optionality::nonOptional,
                [this, disabledLibNames](const QFileInfo& pluginFileInfo, PluginInfoPtr pluginInfo)
                {
                    if (!disabledLibNames.contains(libNameFromFileInfo(pluginFileInfo)))
                        return true;
                    storePluginInfo(
                        pluginInfo,
                        PluginInfo::Status::notLoadedBecauseOfBlackList,
                        PluginInfo::Error::noError,
                        lm("Skipped loading plugin [%1]: blacklisted by %2").args(
                            pluginFileInfo.absoluteFilePath(), pluginsIni().iniFile()));
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
            PluginInfo::Optionality::optional,
            [this, enabledLibNames](const QFileInfo& pluginFileInfo, PluginInfoPtr pluginInfo)
            {
                if (enabledLibNames.isEmpty()
                    || enabledLibNames.contains(libNameFromFileInfo(pluginFileInfo)))
                {
                    return true;
                }
                storePluginInfo(
                    pluginInfo,
                    PluginInfo::Status::notLoadedBecauseOptional,
                    PluginInfo::Error::noError,
                    lm("Skipped loading optional plugin [%1]: not whitelisted by %2").args(
                        pluginFileInfo.absoluteFilePath(), pluginsIni().iniFile()));
                return false;
            });
    }
}

/**
 * @param pluginHomeDir Can be empty if the plugin does not reside in its dedicated dir.
 */
std::unique_ptr<QLibrary> PluginManager::loadPluginLibrary(
    const QString& pluginHomeDir,
    const QString& libFilename,
    PluginInfoPtr pluginInfo)
{
    #if defined(_WIN32)
        if (!pluginsIni().disablePluginLinkedDllLookup && !pluginHomeDir.isEmpty())
        {
            NX_DEBUG(this, "Calling SetDllDirectoryW(%1)",
                nx::kit::utils::toString(pluginHomeDir));
            if (!SetDllDirectoryW(pluginHomeDir.toStdWString().c_str()))
            {
                NX_ERROR(this, "SetDllDirectoryW(%1) failed",
                    nx::kit::utils::toString(pluginHomeDir));
            }
        }
    #else
        nx::utils::unused(pluginHomeDir);
    #endif

    auto lib = std::make_unique<QLibrary>(libFilename);

    // Flag DeepBindHint forces plugin (the loaded side) to use its functions instead of the same
    // named functions of the Server (the loading side). In Linux it is not so by default.
    QLibrary::LoadHints hints = lib->loadHints();
    hints |= QLibrary::DeepBindHint;
    lib->setLoadHints(hints);

    const bool libLoaded = lib->load();

    #if defined(_WIN32)
        if (!pluginsIni().disablePluginLinkedDllLookup && !pluginHomeDir.isEmpty())
        {
            NX_DEBUG(this, "Calling SetDllDirectoryW(nullptr)");
            if (!SetDllDirectoryW(nullptr))
                NX_ERROR(this, "SetDllDirectoryW(nullptr) failed");
        }
    #endif

    if (libLoaded)
        return lib;

    storePluginInfo(
        pluginInfo,
        PluginInfo::Status::notLoadedBecauseOfError,
        PluginInfo::Error::cannotLoadLibrary,
        lm("Failed loading plugin library [%1]: %2").args(libFilename, lib->errorString()));
    return nullptr;
}

void PluginManager::storePluginInfo(
    PluginInfoPtr pluginInfo,
    PluginInfo::Status loadingStatus,
    PluginInfo::Error errorCode,
    QString statusMessage)
{
    pluginInfo->status = loadingStatus;
    pluginInfo->statusMessage = std::move(statusMessage);
    pluginInfo->errorCode = errorCode;
    if (errorCode == PluginInfo::Error::noError)
        NX_INFO(this, pluginInfo->statusMessage);
    else
        NX_ERROR(this, pluginInfo->statusMessage);
    m_nxPlugins.push_back({pluginInfo, /*plugin*/ nullptr});
}

bool PluginManager::loadNxPlugin(
    const nx::plugins::SettingsHolder& settingsHolder,
    const QString& pluginHomeDir,
    const QString& libFilename,
    const QString& libName,
    PluginInfoPtr pluginInfo)
{
    using namespace nx::sdk;
    using namespace nx::vms::api;
    using namespace nx::vms::server;
    using nx::vms::api::analytics::PluginManifest;

    const auto lib = loadPluginLibrary(pluginHomeDir, libFilename, pluginInfo);
    if (!lib)
        return false;

    if (const auto entryPointFunc = reinterpret_cast<nxpl::Plugin::EntryPointFunc>(
        lib->resolve(nxpl::Plugin::kEntryPointFuncName)))
    {
        // Old entry point found: currently, this is a Storage or Camera plugin.
        if (!loadNxPluginForOldSdk(
            entryPointFunc, settingsHolder, libFilename, libName, pluginInfo))
        {
            lib->unload();
            return false;
        }
    }
    else if (const auto entryPointFunc = reinterpret_cast<IPlugin::EntryPointFunc>(
        lib->resolve(IPlugin::kEntryPointFuncName)))
    {
        // New entry point found: currently, this is an Analytics plugin.
        if (!loadNxPluginForNewSdk(
            entryPointFunc, libFilename, libName, pluginInfo))
        {
            lib->unload();
            return false;
        }
    }
    else
    {
        storePluginInfo(
            pluginInfo,
            PluginInfo::Status::notLoadedBecauseOfError,
            PluginInfo::Error::invalidLibrary,
            lm("Failed loading plugin [%1]: No entry point function %2() or old SDK %3()").args(
                libFilename, IPlugin::kEntryPointFuncName, nxpl::Plugin::kEntryPointFuncName));
        lib->unload();
        return false;
    }

    emit pluginLoaded();
    return true;
}

bool PluginManager::loadNxPluginForOldSdk(
    const nxpl::Plugin::EntryPointFunc entryPointFunc,
    const nx::plugins::SettingsHolder& settingsHolder,
    const QString& libFilename,
    const QString& /*libName*/,
    PluginInfoPtr pluginInfo)
{
    using namespace nx::sdk;
    using namespace nx::vms::api;
    using namespace nx::vms::server;
    using nx::vms::api::analytics::PluginManifest;

    const auto plugin = entryPointFunc();
    if (!plugin)
    {
        storePluginInfo(
            pluginInfo,
            PluginInfo::Status::notLoadedBecauseOfError,
            PluginInfo::Error::libraryFailure,
            lm("Failed loading plugin [%1]: old SDK entry point function returned null").args(
                libFilename));
        return false;
    }

    static const QString kPluginLoadedMessageFormat =
        "Loaded plugin [" + libFilename + "]; old SDK, main interface %1";

    pluginInfo->status = PluginInfo::Status::loaded;
    pluginInfo->statusMessage = lm(kPluginLoadedMessageFormat).arg("nxpl::PluginInterface");
    pluginInfo->mainInterface = PluginInfo::MainInterface::nxpl_PluginInterface;

    NX_INFO(this, "Loaded plugin [%1]; old SDK", libFilename);

    if (const auto plugin1 = queryInterfacePtr<nxpl::Plugin>(plugin, nxpl::IID_Plugin))
    {
        pluginInfo->name = plugin1->name();
        pluginInfo->statusMessage = lm(kPluginLoadedMessageFormat).arg("nxpl::Plugin");
        pluginInfo->mainInterface = PluginInfo::MainInterface::nxpl_Plugin;

        // Pass Mediaserver settings (aka "roSettings") to the plugin.
        if (!settingsHolder.isEmpty())
            plugin1->setSettings(settingsHolder.array(), settingsHolder.size());
    }

    if (const auto plugin2 = queryInterfacePtr<nxpl::Plugin2>(plugin, nxpl::IID_Plugin2))
    {
        pluginInfo->name = plugin2->name();
        pluginInfo->statusMessage = lm(kPluginLoadedMessageFormat).arg("nxpl::Plugin2");
        pluginInfo->mainInterface = PluginInfo::MainInterface::nxpl_Plugin2;
        plugin2->setPluginContainer(reinterpret_cast<nxpl::PluginInterface*>(
            makePtr<nx::vms::server::plugins::UtilityProvider>(this, /*plugin*/ nullptr).get()));
    }

    m_nxPlugins.push_back({pluginInfo, toPtr(reinterpret_cast<IRefCountable*>(plugin))});
    return true;
}

namespace {

/** On error, fills PluginInfo and logs the message. */
class ManifestLogger: public nx::vms::server::sdk_support::AbstractManifestLogger
{
public:
    using PluginInfo = nx::vms::api::PluginInfo;
    using PluginInfoPtr = std::shared_ptr<PluginInfo>;

    ManifestLogger(QString libFilename, PluginInfoPtr pluginInfo):
        m_libFilename(std::move(libFilename)), m_pluginInfo(std::move(pluginInfo))
    {
    }

    virtual void log(
        const QString& manifestStr,
        const nx::vms::server::sdk_support::Error& error) override
    {
        if (error.errorCode == nx::sdk::ErrorCode::noError)
            return;

        const QString errorStr = (error.errorCode != nx::sdk::ErrorCode::otherError)
            ? lm(" (Error: [%1] %2)").args(error.errorCode, error.errorMessage)
            : "";

        m_pluginInfo->errorCode = PluginInfo::Error::badManifest;
        m_pluginInfo->status = PluginInfo::Status::notLoadedBecauseOfError;
        m_pluginInfo->statusMessage = lm("Failed loading plugin [%1]: Bad manifest%2").args(
            m_libFilename, errorStr);

        QString logMessage = m_pluginInfo->statusMessage;
        if (!manifestStr.isEmpty())
        {
            logMessage.append("\n");
            logMessage.append("Manifest lines (enquoted and escaped):\n");
            for (const auto& line: manifestStr.split('\n'))
                logMessage.append(QLatin1String((nx::kit::utils::toString(line) + "\n").c_str()));
        }
        NX_ERROR(typeid(PluginManager), logMessage);
    }

private:
    QString m_libFilename;
    PluginInfoPtr m_pluginInfo;
};

} // namespace

bool PluginManager::loadNxPluginForNewSdk(
    const nx::sdk::IPlugin::EntryPointFunc entryPointFunc,
    const QString& libFilename,
    const QString& libName,
    PluginInfoPtr pluginInfo)
{
    using namespace nx::sdk;
    using namespace nx::vms::api;
    using namespace nx::vms::server;
    using nx::vms::api::analytics::PluginManifest;

    const auto plugin = toPtr(entryPointFunc());
    if (!plugin)
    {
        storePluginInfo(
            pluginInfo,
            PluginInfo::Status::notLoadedBecauseOfError,
            PluginInfo::Error::libraryFailure,
            lm("Failed loading plugin [%1]: entry point function returned null").args(
                libFilename));
        return false;
    }

    if (!queryInterfacePtr<nx::sdk::IPlugin>(plugin))
    {
        storePluginInfo(
            pluginInfo,
            PluginInfo::Status::notLoadedBecauseOfError,
            PluginInfo::Error::libraryFailure,
            lm("Failed loading plugin [%1]: interface nx::sdk::IPlugin is not supported").args(
                libFilename));
        return false;
    }

    const char* const name = plugin->name();
    if (!name)
    {
        storePluginInfo(
            pluginInfo,
            PluginInfo::Status::notLoadedBecauseOfError,
            PluginInfo::Error::libraryFailure,
            lm("Failed loading plugin [%1]: name() returned null").args(libFilename));
        return false;
    }

    if (name[0] == '\0')
    {
        storePluginInfo(
            pluginInfo,
            PluginInfo::Status::notLoadedBecauseOfError,
            PluginInfo::Error::libraryFailure,
            lm("Failed loading plugin [%1]: name() returned an empty string").args(libFilename));
        return false;
    }

    static const QString kPluginLoadedMessageFormat =
        "Loaded plugin [" + libFilename + "]; highest supported interface: %1";

    pluginInfo->name = name;
    pluginInfo->status = PluginInfo::Status::loaded;
    pluginInfo->statusMessage = lm(kPluginLoadedMessageFormat).arg("nx::sdk::IPlugin");
    pluginInfo->mainInterface = PluginInfo::MainInterface::nx_sdk_IPlugin;

    NX_INFO(this, "Loaded plugin [%1]", libFilename);

    if (const auto analyticsPlugin = queryInterfacePtr<nx::sdk::analytics::IPlugin>(plugin))
    {
        pluginInfo->statusMessage =
            lm(kPluginLoadedMessageFormat).arg("nx::sdk::analytics::IPlugin");
        pluginInfo->mainInterface = PluginInfo::MainInterface::nx_sdk_analytics_IPlugin;

        if (name != libName)
        {
            NX_WARNING(this, "Analytics plugin name [%1] does not equal library name [%2]",
                name, libName);
        }

        if (const auto manifest = sdk_support::manifestFromSdkObject<PluginManifest>(
            analyticsPlugin, std::make_unique<ManifestLogger>(libFilename, pluginInfo)))
        {
            pluginInfo->name = manifest->name;
            pluginInfo->description = manifest->description;
            pluginInfo->version = manifest->version;
            pluginInfo->vendor = manifest->vendor;
        }
        else
        {
            m_nxPlugins.push_back({pluginInfo, /*plugin*/ nullptr});
            return false; //< The error is already logged.
        }
    }

    plugin->setUtilityProvider(
        makePtr<nx::vms::server::plugins::UtilityProvider>(this, plugin.get()).get());
    m_nxPlugins.push_back({pluginInfo, plugin});
    return true;
}

nx::vms::api::PluginInfoList PluginManager::pluginInfoList() const
{
    if (m_cachedPluginInfo.empty())
    {
        for (const auto& pluginContext: m_nxPlugins)
            m_cachedPluginInfo.push_back(*pluginContext.pluginInfo);
    }

    return m_cachedPluginInfo;
}

std::shared_ptr<const nx::vms::api::PluginInfo> PluginManager::pluginInfo(
    const nx::sdk::IPlugin* plugin) const
{
    if (!NX_ASSERT(plugin))
        return nullptr;

    for (const auto& pluginContext: m_nxPlugins)
    {
        if (pluginContext.plugin.get() == plugin)
            return pluginContext.pluginInfo;
    }

    NX_ERROR(this, "PluginInfo not found for plugin [%1]", plugin->name());
    return nullptr;
}
