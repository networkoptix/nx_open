#include "plugin_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>

#include <plugins/plugin_api.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/i_plugin.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/vms/server/plugins/utility_provider.h>

#include "vms_server_plugins_ini.h"

using namespace nx::sdk;

static QStringList stringToList(const QString& s)
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

static QFileInfoList pluginFileInfoList(const QString& dirToSearchIn, bool searchInnerDirs = true)
{
    QDir pluginDir(dirToSearchIn);
    QDir::Filters filters = QDir::Files | QDir::Readable;
    QFileInfoList filteredEntries;

    if (searchInnerDirs)
        filters |= QDir::Dirs | QDir::NoDotAndDotDot;

    const auto entries = pluginDir.entryInfoList(/*nameFilters*/ QStringList(), filters);

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

void PluginManager::loadPluginsFromDirWithBlackList(
    const QStringList& disabledLibNames,
    const nx::plugins::SettingsHolder& settingsHolder,
    const QString& dirToSearchIn)
{
    for (const auto& fileInfo: pluginFileInfoList(dirToSearchIn))
    {
        const QString& libName = libNameFromFileInfo(fileInfo);

        if (disabledLibNames.contains(libName))
        {
            NX_WARNING(this) << lm("Skipped loading Nx plugin [%1] (blacklisted by %2)")
                .args(fileInfo.absoluteFilePath(), pluginsIni().iniFile());
            continue;
        }

        loadNxPlugin(settingsHolder, fileInfo.absoluteFilePath(), libName);
        // Ignore return value - an error is already logged.
    }
}

/**
 * @param enabledLibNames If empty, all plugins are loaded.
 */
void PluginManager::loadPluginsFromDirWithWhiteList(
    const QStringList& enabledLibNames,
    const nx::plugins::SettingsHolder& settingsHolder,
    const QString& dirToSearchIn)
{
    for (const auto& fileInfo: pluginFileInfoList(dirToSearchIn))
    {
        const QString& libName = libNameFromFileInfo(fileInfo);

        if (!enabledLibNames.isEmpty() && !enabledLibNames.contains(libName))
        {
            NX_WARNING(this) << lm("Skipped loading Nx plugin [%1] (not whitelisted by %2)")
                .args(fileInfo.absoluteFilePath(), pluginsIni().iniFile());
            continue;
        }

        loadNxPlugin(settingsHolder, fileInfo.absoluteFilePath(), libName);
        // Ignore return value - an error is already logged.
    }
}

void PluginManager::loadPlugins(const QSettings* settings)
{
    QnMutexLocker lock(&m_mutex);

    std::set<QString> directoriesToSearchForPlugins;

    const char* const vmsPluginDir = getenv("VMS_PLUGIN_DIR");
    if (vmsPluginDir)
        directoriesToSearchForPlugins.insert(QString::fromLatin1(vmsPluginDir));

    const QString binPath = QDir(QCoreApplication::applicationDirPath()).absolutePath();
    directoriesToSearchForPlugins.insert(binPath + lit("/plugins/"));

    const QString optionalPluginsDir = binPath + lit("/plugins_optional/");

    const nx::plugins::SettingsHolder settingsHolder{settings};
    NX_ASSERT(settingsHolder.isValid());

    // Load regular plugins, if not prohibited by .ini.
    const QString disabledNxPlugins =
        QString::fromLatin1(pluginsIni().disabledNxPlugins).trimmed();
    if (disabledNxPlugins != lit("*"))
    {
        const QStringList disabledLibNames = stringToList(disabledNxPlugins);
        const QStringList enabledLibNames{};

        for (const QString& dir: directoriesToSearchForPlugins)
            loadPluginsFromDirWithBlackList(disabledLibNames, settingsHolder, dir);
    }
    else
    {
        NX_WARNING(this) << lm("Skipped loading all non-optional Nx plugins (as per %2)")
            .args(pluginsIni().iniFile());
    }

    // Load optional plugins, if demanded by .ini.
    const QString enabledNxPluginsOptional =
        QString::fromLatin1(pluginsIni().enabledNxPluginsOptional).trimmed();
    if (!enabledNxPluginsOptional.isEmpty())
    {
        NX_WARNING(this) << lm("Loading optional Nx plugins, if any (as per %2)")
            .args(pluginsIni().iniFile());

        const QStringList disabledLibNames{};
        const QStringList enabledLibNames = (enabledNxPluginsOptional == lit("*"))
            ? QStringList()
            : stringToList(enabledNxPluginsOptional);

        loadPluginsFromDirWithWhiteList(enabledLibNames, settingsHolder, optionalPluginsDir);
    }
}

bool PluginManager::loadNxPlugin(
    const nx::plugins::SettingsHolder& settingsHolder,
    const QString& filename,
    const QString& libName)
{
    QLibrary lib(filename);
    // Flag DeepBindHint forces plugin (the loaded side) to use its functions instead of the same
    // named functions of the Server (the loading side). In Linux it is not so by default.
    QLibrary::LoadHints hints = lib.loadHints();
    hints |= QLibrary::DeepBindHint;
    lib.setLoadHints(hints);
    if (!lib.load())
    {
        NX_ERROR(this) << lit("Failed to load Nx plugin [%1]: %2")
            .arg(filename).arg(lib.errorString());
        return false;
    }

    if (const auto entryPointFunc = reinterpret_cast<nxpl::Plugin::EntryPointFunc>(
        lib.resolve(nxpl::Plugin::kEntryPointFuncName)))
    {
        // Old entry point found: currently, this is a Storage or Camera plugin.

        const auto plugin = entryPointFunc();
        if (!plugin)
        {
            NX_ERROR(this, "Failed to load Nx old SDK plugin [%1]: entry function returned null",
                filename);
            lib.unload();
            return false;
        }
        m_nxPlugins.emplace_back(reinterpret_cast<IRefCountable*>(plugin));
        NX_WARNING(this, "Loaded Nx old SDK plugin [%1]", filename);

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
        lib.resolve(IPlugin::kEntryPointFuncName)))
    {
        // New entry point found: currently, this is an Analytics plugin.

        const auto plugin = toPtr(entryPointFunc());
        if (!plugin)
        {
            NX_ERROR(this, "Failed to load Nx plugin [%1]: entry function returned null",
                filename);
            lib.unload();
            return false;
        }
        m_nxPlugins.emplace_back(plugin);
        NX_WARNING(this, "Loaded Nx plugin [%1]", filename);

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
            filename, IPlugin::kEntryPointFuncName, nxpl::Plugin::kEntryPointFuncName);
        lib.unload();
        return false;
    }

    emit pluginLoaded();
    return true;
}
