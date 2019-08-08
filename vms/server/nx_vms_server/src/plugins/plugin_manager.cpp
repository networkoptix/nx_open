#include "plugin_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <nx/utils/switch.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/kit/utils.h>
#include <plugins/plugin_api.h>
#include <nx/sdk/helpers/lib_context.h>
#include <nx/vms/server/sdk_support/ref_countable_registry.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/vms/server/plugins/utility_provider.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/vms/server/analytics/wrappers/plugin.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

#include <media_server/media_server_module.h>

#include "vms_server_plugins_ini.h"
#include "plugin_loading_context.h"

using namespace nx::sdk;
using nx::vms::server::sdk_support::RefCountableRegistry;
using nx::vms::api::PluginInfo;
using namespace nx::vms::server::analytics;

static QStringList stringToListViaComma(const QString& s)
{
    QStringList list = s.split(",");
    for (auto& item: list)
        item = item.trimmed();
    return list;
}

PluginManager::PluginManager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
    libContext().setName("nx_vms_server");
    libContext().setRefCountableRegistry(
        RefCountableRegistry::createIfEnabled(libContext().name()));
}

PluginManager::~PluginManager()
{
}

void PluginManager::unloadPlugins()
{
    QnMutexLocker lock(&m_mutex);
    m_pluginContexts.clear();
}

nx::vms::api::PluginInfoList PluginManager::pluginInfoList() const
{
    QnMutexLocker lock(&m_mutex);
    if (m_cachedPluginInfo.empty())
    {
        for (const auto& pluginContext: m_pluginContexts)
            m_cachedPluginInfo.push_back(*pluginContext.pluginInfo);
    }

    return m_cachedPluginInfo;
}

std::shared_ptr<const PluginInfo> PluginManager::pluginInfo(
    const IPlugin* plugin) const
{
    if (!NX_ASSERT(plugin))
        return nullptr;

    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& pluginContext : m_pluginContexts)
        {
            if (pluginContext.plugin.get() == plugin)
                return pluginContext.pluginInfo;
        }
    }

    const QString errorMessage = "PluginManager: PluginInfo not found for plugin instance @"
        + QString::fromLatin1(nx::kit::utils::toString(plugin).c_str());

    NX_ASSERT(false, errorMessage);

    const auto internalErrorPluginInfo = std::make_shared<PluginInfo>();
    internalErrorPluginInfo->errorCode = Error::internalError;
    internalErrorPluginInfo->statusMessage = "INTERNAL ERROR: " + errorMessage;
    return internalErrorPluginInfo;
}

void PluginManager::setIsActive(const IRefCountable* plugin, bool isActive)
{
    if (!NX_ASSERT(plugin))
        return;

    QnMutexLocker lock(&m_mutex);
    for (auto& pluginContext: m_pluginContexts)
    {
        if (pluginContext.plugin.get() != plugin)
            continue;

        if (pluginContext.pluginInfo->isActive == isActive)
            return;

        pluginContext.pluginInfo->isActive = isActive;
        m_cachedPluginInfo.clear();
        return;
    }
}

//-------------------------------------------------------------------------------------------------
// Loading plugins

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

static QFileInfoList pluginFileInfoList(const QDir& dirToSearch, bool searchInnerDirs = true)
{
    QDir::Filters filters = QDir::Files | QDir::Readable;
    QFileInfoList filteredEntries;

    if (searchInnerDirs)
        filters |= QDir::Dirs | QDir::NoDotAndDotDot;

    for (const auto& entry: dirToSearch.entryInfoList(/*nameFilters*/ QStringList(), filters))
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
                const QDir entryAsDir(entry.absoluteFilePath());
                for (const auto& pluginDirEntry: entryAsDir.entryInfoList(
                    /*nameFilters*/ QStringList(), QDir::Files | QDir::Readable))
                {
                    if (libNameFromFileInfo(pluginDirEntry) == entry.fileName())
                    {
                        filteredEntries << pluginDirEntry;
                        break;
                    }
                }
            }
        }
        else //< Entry is a file.
        {
            if (!libNameFromFileInfo(entry).isEmpty())
                filteredEntries << entry;  //< It is a plugin entry.
        }
    }
    return filteredEntries;
}

static QString mainInterfaceMessage(PluginInfo::MainInterface mainInterface)
{
    if (mainInterface == PluginInfo::MainInterface::undefined)
        return QString();

    const bool isMainInterfaceOldSdk = nx::utils::switch_(mainInterface,
        PluginInfo::MainInterface::nxpl_PluginInterface, [] { return true; },
        PluginInfo::MainInterface::nxpl_Plugin, [] { return true; },
        PluginInfo::MainInterface::nxpl_Plugin2, [] { return true; },
        nx::utils::default_, [] { return false; }
    );

    return lm(" (%1main interface %2)").args(
        isMainInterfaceOldSdk ? "old SDK, " : "",
        mainInterface);
}

void PluginManager::storeInternalErrorPluginInfo(
    PluginInfoPtr pluginInfo, Ptr<IRefCountable> plugin, const QString& errorMessage)
{
    QString originalPluginInfoDescription;
    if (pluginInfo)
    {
        originalPluginInfoDescription =
            lm("Original PluginInfo fields: errorCode [%1], statusMessage %2.").args(
                pluginInfo->errorCode, nx::kit::utils::toString(pluginInfo->statusMessage));
    }
    else
    {
        pluginInfo.reset(new PluginInfo);
        originalPluginInfoDescription = "Original PluginInfo is null.";
    }

    NX_ASSERT(!errorMessage.isEmpty());

    pluginInfo->statusMessage =
        lm("INTERNAL ERROR: %1 %2").args(errorMessage, originalPluginInfoDescription);

    pluginInfo->errorCode = Error::internalError;

    m_pluginContexts.push_back({pluginInfo, plugin});
}

/** @return Always false, to enable usage `return storeNotLoadedPluginInfo(...);`. */
bool PluginManager::storeNotLoadedPluginInfo(
    PluginInfoPtr pluginInfo, Status status, Error errorCode, const QString& reason)
{
    const bool isOk = errorCode == Error::noError;
    if (!NX_ASSERT(pluginInfo)
        || !NX_ASSERT(isOk == (status != Status::notLoadedBecauseOfError))
        || !NX_ASSERT(!reason.isEmpty())
        || !NX_ASSERT(!pluginInfo->libName.isEmpty())
        || !NX_ASSERT(!pluginInfo->libraryFilename.isEmpty())
        || !NX_ASSERT(pluginInfo->statusMessage.isEmpty())
    )
    {
        storeInternalErrorPluginInfo(pluginInfo, /*plugin*/ nullptr,
            "Server Plugin was not loaded and an assertion has failed - see the Server log.");
        return false;
    }

    pluginInfo->status = status;
    pluginInfo->errorCode = errorCode;

    pluginInfo->statusMessage = lm("%1%2 Server plugin [%3]%4: %5").args(
        isOk ? "Skipped loading" : "Failed loading",
        pluginInfo->optionality == Optionality::optional ? " optional" : "",
        pluginInfo->libraryFilename,
        mainInterfaceMessage(pluginInfo->mainInterface),
        isOk ? reason : QString(lm("[%1]: %2").args(errorCode, reason)));

    if (isOk)
        NX_INFO(this, pluginInfo->statusMessage);
    else
        NX_ERROR(this, pluginInfo->statusMessage);

    m_pluginContexts.push_back({pluginInfo, /*plugin*/ nullptr});
    return false;
}

/** @return Always true, to enable usage `return storeLoadedPluginInfo(...);`. */
bool PluginManager::storeLoadedPluginInfo(
    PluginInfoPtr pluginInfo, Ptr<IRefCountable> plugin)
{
    if (!NX_ASSERT(plugin) || !NX_ASSERT(pluginInfo)
        || !NX_ASSERT(!pluginInfo->libName.isEmpty())
        || !NX_ASSERT(!pluginInfo->libraryFilename.isEmpty())
        || !NX_ASSERT(pluginInfo->errorCode == Error::noError)
        || !NX_ASSERT(pluginInfo->status == Status::loaded)
        || !NX_ASSERT(pluginInfo->statusMessage.isEmpty())
        || !NX_ASSERT(pluginInfo->mainInterface != MainInterface::undefined)
    )
    {
        storeInternalErrorPluginInfo(pluginInfo, plugin,
            "Server Plugin was loaded but an assertion has failed - see the Server log.");
        return true;
    }

    pluginInfo->statusMessage = lm("Loaded%1 Server plugin [%2]%3").args(
        pluginInfo->optionality == Optionality::optional ? " optional" : "",
        pluginInfo->libraryFilename,
        mainInterfaceMessage(pluginInfo->mainInterface));

    NX_INFO(this, pluginInfo->statusMessage);

    m_pluginContexts.push_back({pluginInfo, plugin});
    return true;
}

bool PluginManager::processPluginEntryPointForNewSdk(
    IPlugin::EntryPointFunc entryPointFunc, PluginInfoPtr pluginInfo)
{
    const auto error = //< Returns false to enable usage `return error(...)`.
        [this, &pluginInfo](const QString& errorMessage)
        {
            return storeNotLoadedPluginInfo(pluginInfo, Status::notLoadedBecauseOfError,
                Error::libraryFailure, errorMessage);
        };

    const auto plugin = toPtr(entryPointFunc());
    if (!plugin)
        return error("Entry point function returned null");

    if (!plugin->queryInterface<IPlugin>())
        return error("Interface nx::sdk::IPlugin is not supported");

    pluginInfo->mainInterface = MainInterface::nx_sdk_IPlugin;

    NX_ASSERT(!pluginInfo->libName.isEmpty());
    if (const auto analyticsPlugin = plugin->queryInterface<nx::sdk::analytics::IPlugin>())
    {
        const auto plugin = std::make_shared<wrappers::Plugin>(
            serverModule(),
            analyticsPlugin,
            pluginInfo->libName);

        pluginInfo->mainInterface = MainInterface::nx_sdk_analytics_IPlugin;
        if (const auto manifest = plugin->manifest())
        {
            pluginInfo->name = manifest->name;
            pluginInfo->description = manifest->description;
            pluginInfo->version = manifest->version;
            pluginInfo->vendor = manifest->vendor;
        }
        else
        {
            return storeNotLoadedPluginInfo(pluginInfo, Status::notLoadedBecauseOfError,
                Error::badManifest, "Invalid manifest");
        }
    }

    plugin->setUtilityProvider(
        makePtr<nx::vms::server::plugins::UtilityProvider>(this, plugin.get()).get());

    return storeLoadedPluginInfo(pluginInfo, plugin);
}

bool PluginManager::processPluginEntryPointForOldSdk(
    nxpl::Plugin::EntryPointFunc entryPointFunc,
    const SettingsHolder& settingsHolder,
    PluginInfoPtr pluginInfo)
{
    const auto plugin = entryPointFunc();
    if (!plugin)
    {
        return storeNotLoadedPluginInfo(pluginInfo, Status::notLoadedBecauseOfError,
            Error::libraryFailure, "Old SDK entry point function returned null");
    }

    pluginInfo->mainInterface = MainInterface::nxpl_PluginInterface;

    if (const auto plugin1 = queryInterfaceOfOldSdk<nxpl::Plugin>(plugin, nxpl::IID_Plugin))
    {
        pluginInfo->mainInterface = MainInterface::nxpl_Plugin;

        // Allow old SDK plugins to have null or empty name not to block such plugins.
        const char* const name = plugin1->name();
        if (name == nullptr)
            pluginInfo->name = "<null plugin name>";
        else if (name[0] == '\0')
            pluginInfo->name = "<empty plugin name>";
        else
            pluginInfo->name = name;

        // Pass Mediaserver settings (aka "roSettings") to the plugin.
        if (!settingsHolder.isEmpty())
            plugin1->setSettings(settingsHolder.array(), settingsHolder.size());
    }

    if (const auto plugin2 = queryInterfaceOfOldSdk<nxpl::Plugin2>(plugin, nxpl::IID_Plugin2))
    {
        pluginInfo->mainInterface = MainInterface::nxpl_Plugin2;

        // UtilityProvider supports old SDK's interface nxpl::TimeProvider for compatibility.
        plugin2->setPluginContainer(reinterpret_cast<nxpl::PluginInterface*>(
            makePtr<nx::vms::server::plugins::UtilityProvider>(this, /*plugin*/ nullptr).get()));
    }

    storeLoadedPluginInfo(pluginInfo, toPtr(reinterpret_cast<IRefCountable*>(plugin)));
    return true;
}

bool PluginManager::processPluginLib(
    QLibrary* lib, const SettingsHolder& settingsHolder, PluginInfoPtr pluginInfo)
{
    if (const auto nxLibContextFunc = reinterpret_cast<NxLibContextFunc>(
        lib->resolve(kNxLibContextFuncName)))
    {
        ILibContext* const pluginLibContext = nxLibContextFunc();
        if (!NX_ASSERT(pluginLibContext))
            return false;

        pluginLibContext->setName(pluginInfo->libName.toStdString().c_str());
        pluginLibContext->setRefCountableRegistry(
            RefCountableRegistry::createIfEnabled(pluginInfo->libName.toStdString()));
    }

    if (const auto oldEntryPointFunc = reinterpret_cast<nxpl::Plugin::EntryPointFunc>(
        lib->resolve(nxpl::Plugin::kEntryPointFuncName)))
    {
        // Old entry point found: currently, this is a Storage or Camera plugin.
        return processPluginEntryPointForOldSdk(
            oldEntryPointFunc, settingsHolder, pluginInfo);
    }

    if (const auto entryPointFunc = reinterpret_cast<IPlugin::EntryPointFunc>(
        lib->resolve(IPlugin::kEntryPointFuncName)))
    {
        // New entry point found: currently, this is an Analytics plugin.
        return processPluginEntryPointForNewSdk(entryPointFunc, pluginInfo);
    }

    return storeNotLoadedPluginInfo(pluginInfo, Status::notLoadedBecauseOfError,
        Error::invalidLibrary, lm("No entry point function %1() or old SDK %2()").args(
            IPlugin::kEntryPointFuncName, nxpl::Plugin::kEntryPointFuncName));
}

void PluginManager::loadPlugin(
    const SettingsHolder& settingsHolder, PluginInfoPtr pluginInfo)
{
    NX_INFO(this, "Considering to load Server plugin [%1]", pluginInfo->libraryFilename);

    [[maybe_unused]] /* RAII-only object */ const PluginLoadingContext pluginLoadingContext(
        this, pluginInfo->homeDir, pluginInfo->libName);

    // NOTE: QLibrary does not unload the dynamic library in its destructor.
    auto lib = std::make_unique<QLibrary>(pluginInfo->libraryFilename);

    // Flag DeepBindHint forces plugin (the loaded side) to use its functions instead of the same
    // named functions of the Server (the loading side). In Linux it is not so by default.
    QLibrary::LoadHints hints = lib->loadHints();
    hints |= QLibrary::DeepBindHint;
    lib->setLoadHints(hints);

    if (!lib->load())
    {
        storeNotLoadedPluginInfo(pluginInfo, Status::notLoadedBecauseOfError,
            Error::cannotLoadLibrary, lib->errorString());
        return;
    }

    if (!processPluginLib(lib.get(), settingsHolder, pluginInfo))
    {
        lib->unload();
        return;
    }

    emit pluginLoaded();
}

void PluginManager::loadPluginsFromDir(
    const SettingsHolder& settingsHolder,
    const QDir& dirToSearch,
    Optionality optionality,
    std::function<bool(PluginInfoPtr pluginInfo)> allowPlugin)
{
    for (const auto& fileInfo: pluginFileInfoList(dirToSearch))
    {
        const PluginInfoPtr pluginInfo = std::make_shared<PluginInfo>();
        pluginInfo->libName = libNameFromFileInfo(fileInfo);
        pluginInfo->libraryFilename = fileInfo.absoluteFilePath();

        // The plugin resides either in its dedicated directory, or in dirToSearch (with others).
        pluginInfo->homeDir = (fileInfo.dir() != dirToSearch) ? fileInfo.dir().absolutePath() : "";

        pluginInfo->optionality = optionality;

        if (allowPlugin(pluginInfo))
            loadPlugin(settingsHolder, pluginInfo);
    }
}

void PluginManager::loadOptionalPluginsIfNeeded(
    const QString& binPath, const SettingsHolder& settingsHolder)
{
    // Load optional plugins, if demanded by .ini.
    const QString enabledNxPluginsOptional =
        QString::fromLatin1(pluginsIni().enabledNxPluginsOptional).trimmed();
    if (enabledNxPluginsOptional.isEmpty())
        return;

    NX_INFO(this, "Loading optional Server plugins, if any (as per %1)",
        pluginsIni().iniFile());

    const QString optionalPluginsDir = binPath + "/plugins_optional/";

    const QStringList enabledLibNames = (enabledNxPluginsOptional == "*")
        ? QStringList() //< Empty list will mean "all optional plugins are enabled".
        : stringToListViaComma(enabledNxPluginsOptional);

    loadPluginsFromDir(settingsHolder, optionalPluginsDir, Optionality::optional,
        [this, &enabledLibNames](PluginInfoPtr pluginInfo)
        {
            if (enabledLibNames.isEmpty() || enabledLibNames.contains(pluginInfo->libName))
                return true;
            return storeNotLoadedPluginInfo(pluginInfo, Status::notLoadedBecauseOptional,
                Error::noError, lm("Not whitelisted by %1").arg(pluginsIni().iniFile()));
        });
}

void PluginManager::loadNonOptionalPluginsIfNeeded(
    const QString& binPath, const SettingsHolder& settingsHolder)
{
    const QString disabledNxPlugins =
        QString::fromLatin1(pluginsIni().disabledNxPlugins).trimmed();

    if (disabledNxPlugins == "*")
    {
        NX_INFO(this, "Skipped loading all non-optional Server plugins, as per %1",
            pluginsIni().iniFile());
        return;
    }

    std::set<QString> directoriesToSearchForPlugins;

    const char* const vmsPluginDir = getenv("VMS_PLUGIN_DIR");
    if (vmsPluginDir)
        directoriesToSearchForPlugins.insert(QString::fromLatin1(vmsPluginDir));

    directoriesToSearchForPlugins.insert(binPath + "/plugins/");

    const QStringList disabledLibNames = stringToListViaComma(disabledNxPlugins);

    for (const QString& dir: directoriesToSearchForPlugins)
    {
        loadPluginsFromDir(settingsHolder, dir, Optionality::nonOptional,
            [this, &disabledLibNames](PluginInfoPtr pluginInfo)
            {
                if (!disabledLibNames.contains(pluginInfo->libName))
                    return true;
                return storeNotLoadedPluginInfo(pluginInfo, Status::notLoadedBecauseOfBlackList,
                    Error::noError, lm("Blacklisted by %2").arg(pluginsIni().iniFile()));
            });
    }
}

void PluginManager::loadPlugins(const QSettings* settings)
{
    QnMutexLocker lock(&m_mutex);

    const SettingsHolder settingsHolder{settings};
    if (!NX_ASSERT(settingsHolder.isValid()))
        return;

    const QString binPath = QDir(QCoreApplication::applicationDirPath()).absolutePath();

    loadNonOptionalPluginsIfNeeded(binPath, settingsHolder);
    loadOptionalPluginsIfNeeded(binPath, settingsHolder);
}
