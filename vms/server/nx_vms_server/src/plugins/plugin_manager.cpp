#include "plugin_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>

#include <camera/camera_plugin.h>
#include <nx/sdk/analytics/i_engine.h>
#include <plugins/plugin_tools.h>

#include "plugins_ini.h"

using namespace nx::sdk;

static QStringList stringToList(const QString& s)
{
    QStringList list = s.split(lit(","));
    for (auto& item: list)
        item = item.trimmed();
    return list;
}

PluginManager::PluginManager(QObject* parent, nxpl::PluginInterface* pluginContainer):
    QObject(parent), m_pluginContainer(pluginContainer)
{
}

PluginManager::~PluginManager()
{
    for (auto& qtPlugin: m_qtPlugins)
        qtPlugin->unload();

    for (auto& nxPlugin: m_nxPlugins)
        nxPlugin->releaseRef();
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

static QFileInfoList pluginFileInfoList(const QString& dirToSearchIn)
{
    QDir pluginDir(dirToSearchIn);
    const auto entries = pluginDir.entryInfoList(QStringList(), QDir::Files | QDir::Readable);
    QFileInfoList filteredEntries;
    for (const auto& entry: entries)
    {
        if (!libNameFromFileInfo(entry).isEmpty())
            filteredEntries << entry;  //< It is a plugin entry.
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
        // Ignore return value - an error is aleady logged.
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
        // Ignore return value - an error is aleady logged.
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
    // named functions of server (the loading side). In Linux it is no so by default.
    QLibrary::LoadHints hints = lib.loadHints();
    hints |= QLibrary::DeepBindHint;
    lib.setLoadHints(hints);
    if (!lib.load())
    {
        NX_ERROR(this) << lit("Failed to load Nx plugin [%1]: %2")
            .arg(filename).arg(lib.errorString());
        return false;
    }

    // TODO: Find a better solution. Needed to check that libName equals IPlugin::name().
    bool isAnalyticsPlugin = false;

    auto entryPoint = (nxpl::Plugin::EntryPoint) lib.resolve("createNXPluginInstance");
    if (entryPoint == nullptr)
    {
        entryPoint = (nxpl::Plugin::EntryPoint) lib.resolve("createNxAnalyticsPlugin");
        if (entryPoint)
            isAnalyticsPlugin = true;
    }
    if (entryPoint == nullptr)
    {
        NX_ERROR(this, "Failed to load Nx plugin [%1]: "
            "Neither createNXPluginInstance() nor createNxAnalyticsPlugin() functions found",
            filename);
        lib.unload();
        return false;
    }

    nxpl::PluginInterface* obj = entryPoint();
    if (!obj)
    {
        NX_ERROR(this, "Failed to load Nx plugin [%1]: entry function returned null", filename);
        lib.unload();
        return false;
    }

    NX_WARNING(this, "Loaded Nx plugin [%1]", filename);
    m_nxPlugins.push_back(obj);

    if (auto pluginObj = nxpt::ScopedRef<nxpl::Plugin>(obj->queryInterface(nxpl::IID_Plugin)))
    {
        if (isAnalyticsPlugin && pluginObj->name() != libName)
        {
            NX_WARNING(this, "Analytics plugin name [%1] does not equal library name [%2]",
                pluginObj->name(), libName);
        }

        // Pass Mediaserver settings (aka "roSettings") to the plugin.
        if (!settingsHolder.isEmpty())
            pluginObj->setSettings(settingsHolder.array(), settingsHolder.size());
    }

    if (auto plugin2Obj = nxpt::ScopedRef<nxpl::Plugin2>(obj->queryInterface(nxpl::IID_Plugin2)))
    {
        if (m_pluginContainer)
            plugin2Obj->setPluginContainer(m_pluginContainer);
    }

    emit pluginLoaded();
    return true;
}
