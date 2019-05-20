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

#include "vms_server_plugins_ini.h"

using namespace nx::sdk;

static QStringList stringToListViaComma(const QString& s)
{
    QStringList list = s.split(lit(","));
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

    QString libName = fileInfo.baseName().left(fileInfo.baseName().lastIndexOf(lit(".")));
    static const auto kPrefix = lit("lib");
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
    std::function<bool(const QFileInfo& pluginFileInfo)> allowPlugin)
{
    for (const auto& fileInfo: pluginFileInfoList(dirToSearchIn))
    {
        const QString& libName = libNameFromFileInfo(fileInfo);

        if (!allowPlugin(fileInfo))
            continue;

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
            libName);
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
    if (disabledNxPlugins != lit("*"))
    {
        std::set<QString> directoriesToSearchForPlugins;

        const char* const vmsPluginDir = getenv("VMS_PLUGIN_DIR");
        if (vmsPluginDir)
            directoriesToSearchForPlugins.insert(QString::fromLatin1(vmsPluginDir));

        directoriesToSearchForPlugins.insert(binPath + lit("/plugins/"));

        const QStringList disabledLibNames = stringToListViaComma(disabledNxPlugins);
        for (const QString& dir: directoriesToSearchForPlugins)
        {
            loadPluginsFromDir(settingsHolder, dir,
                [this, disabledLibNames](const QFileInfo& pluginFileInfo)
                {
                    if (!disabledLibNames.contains(libNameFromFileInfo(pluginFileInfo)))
                        return true;
                    NX_WARNING(this) << lm("Skipped loading Nx plugin [%1] (blacklisted by %2)")
                        .args(pluginFileInfo.absoluteFilePath(), pluginsIni().iniFile());
                    return false;
                });
        }
    }
    else
    {
        NX_WARNING(this) << lm("Skipped loading all non-optional Nx plugins (as per %2)")
            .args(pluginsIni().iniFile());
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
        NX_WARNING(this) << lm("Loading optional Nx plugins, if any (as per %2)")
            .args(pluginsIni().iniFile());

        const QString optionalPluginsDir = binPath + lit("/plugins_optional/");
        const QStringList enabledLibNames = (enabledNxPluginsOptional == lit("*"))
            ? QStringList()
            : stringToListViaComma(enabledNxPluginsOptional);

        loadPluginsFromDir(settingsHolder, optionalPluginsDir,
            [this, enabledLibNames](const QFileInfo& pluginFileInfo)
            {
                if (enabledLibNames.isEmpty()
                    || enabledLibNames.contains(libNameFromFileInfo(pluginFileInfo)))
                {
                    return true;
                }
                NX_WARNING(this) << lm("Skipped loading Nx plugin [%1] (not whitelisted by %2)")
                    .args(pluginFileInfo.absoluteFilePath(), pluginsIni().iniFile());
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

bool PluginManager::loadNxPlugin(
    const nx::plugins::SettingsHolder& settingsHolder,
    const QString& linkedLibsDirectory,
    const QString& libFilename,
    const QString& libName)
{
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
            NX_ERROR(this, "Failed to load Nx old SDK plugin [%1]: entry function returned null",
                libFilename);
            lib->unload();
            return false;
        }
        m_nxPlugins.emplace_back(reinterpret_cast<IRefCountable*>(plugin));
        NX_WARNING(this, "Loaded Nx old SDK plugin [%1]", libFilename);

        if (const auto plugin1 = queryInterfacePtr<nxpl::Plugin>(plugin, nxpl::IID_Plugin))
        {
            // Pass Mediaserver settings (aka "roSettings") to the plugin.
            if (!settingsHolder.isEmpty())
                plugin1->setSettings(settingsHolder.array(), settingsHolder.size());
        }

        if (const auto plugin2 = queryInterfacePtr<nxpl::Plugin2>(plugin, nxpl::IID_Plugin2))
        {
            plugin2->setPluginContainer(
                reinterpret_cast<nxpl::PluginInterface*>(m_utilityProvider.get()));
        }
    }
    else if (const auto entryPointFunc = reinterpret_cast<IPlugin::EntryPointFunc>(
        lib->resolve(IPlugin::kEntryPointFuncName)))
    {
        // New entry point found: currently, this is an Analytics plugin.

        const auto plugin = toPtr(entryPointFunc());
        if (!plugin)
        {
            NX_ERROR(this, "Failed to load Nx plugin [%1]: entry function returned null",
                libFilename);
            lib->unload();
            return false;
        }
        m_nxPlugins.emplace_back(plugin);
        NX_WARNING(this, "Loaded Nx plugin [%1]", libFilename);

        if (queryInterfacePtr<nx::sdk::analytics::IPlugin>(plugin))
        {
            if (plugin->name() != libName)
            {
                NX_WARNING(this, "Analytics plugin name [%1] does not equal library name [%2]",
                    plugin->name(), libName);
            }
        }

        plugin->setUtilityProvider(m_utilityProvider.get());
    }
    else
    {
        NX_ERROR(this, "Failed to load Nx plugin [%1]: "
            "Neither %2(), nor the old SDK's %3() functions found",
            libFilename, IPlugin::kEntryPointFuncName, nxpl::Plugin::kEntryPointFuncName);
        lib->unload();
        return false;
    }

    emit pluginLoaded();
    return true;
}
