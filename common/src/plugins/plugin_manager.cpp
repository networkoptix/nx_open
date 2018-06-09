#include "plugin_manager.h"

#include <algorithm>
#include <set>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>

#include <decoders/abstractclientplugin.h>
#include <decoders/abstractvideodecoderplugin.h>
#include <camera/camera_plugin.h>
#include <nx/sdk/metadata/plugin.h>
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

PluginManager::PluginManager(
    QObject* parent,
    const QString& pluginDir,
    nxpl::PluginInterface* pluginContainer)
    :
    QObject(parent),
    m_pluginDir(pluginDir),
    m_pluginContainer(pluginContainer)
{
}

PluginManager::~PluginManager()
{
    for (auto& qtPlugin: m_qtPlugins)
        qtPlugin->unload();

    for (auto& nxPlugin: m_nxPlugins)
        nxPlugin->releaseRef();
}

void PluginManager::loadPlugins(
    const QSettings* settings,
    PluginManager::PluginType pluginsToLoad)
{
    QnMutexLocker lk(&m_mutex);

    std::set<QString> directoriesToSearchForPlugins;

    // Loading plugins.
    if(!m_pluginDir.isEmpty())
        directoriesToSearchForPlugins.insert(QDir(m_pluginDir).absolutePath());

#ifndef Q_OS_WIN32
    char* vmsPluginDir = getenv("VMS_PLUGIN_DIR");
    if( vmsPluginDir )
        directoriesToSearchForPlugins.insert( QString::fromLatin1(vmsPluginDir) );
#endif

    const QString binPath = QDir(QCoreApplication::applicationDirPath()).absolutePath();
    //directoriesToSearchForPlugins.insert( QDir(QDir::currentPath()).absolutePath() );
    directoriesToSearchForPlugins.insert(binPath + lit("/plugins/"));

    const QString optionalPluginsDir = binPath + lit("/plugins_optional/");

    // Preparing settings for Nx plugins.
    const auto& keys = settings->allKeys();
    std::vector<nxpl::Setting> settingsForPlugin;
    for (const auto& key: keys)
    {
        const auto& keyUtf8 = key.toUtf8();
        const auto& valueUtf8 = settings->value(key).toString().toUtf8();

        nxpl::Setting setting;
        setting.name = new char[keyUtf8.size() + 1];
        strcpy(setting.name, keyUtf8.constData());
        setting.value = new char[valueUtf8.size() + 1];;
        strcpy(setting.value, valueUtf8.constData());
        settingsForPlugin.push_back(std::move(setting));
    }

    // Load regular plugins, if not prohibited by .ini.
    const QString disabledNxPlugins =
        QString::fromLatin1(pluginsIni().disabledNxPlugins).trimmed();
    if (disabledNxPlugins != lit("*"))
    {
        const QStringList disabledLibNames = stringToList(disabledNxPlugins);
        const QStringList enabledLibNames{};

        for (const QString& dir: directoriesToSearchForPlugins)
        {
            loadPluginsFromDir(
                disabledLibNames, enabledLibNames, settingsForPlugin, dir, pluginsToLoad);
        }
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

        loadPluginsFromDir(
            disabledLibNames, enabledLibNames, settingsForPlugin,
            optionalPluginsDir, pluginsToLoad);
    }

    for (nxpl::Setting& setting: settingsForPlugin)
    {
        delete[] setting.name;
        delete[] setting.value;
    }
}

/**
 * @param disabledLibNames If not empty, specified plugins will be skipped.
 * @param enabledLibNames If not empty, only specified plugins will be loaded.
 */
void PluginManager::loadPluginsFromDir(
    const QStringList& disabledLibNames,
    const QStringList& enabledLibNames,
    const std::vector<nxpl::Setting>& settingsForPlugin,
    const QString& dirToSearchIn,
    PluginType pluginsToLoad)
{
    QDir pluginDir(dirToSearchIn);
    const QStringList& entries = pluginDir.entryList(QStringList(), QDir::Files | QDir::Readable);

    for (const QString& entry: entries)
    {
        if (!QLibrary::isLibrary(entry))
            continue;

        if (pluginsToLoad & QtPlugin)
            loadQtPlugin(pluginDir.path() + lit("/") + entry);

        if (pluginsToLoad & NxPlugin)
        {
            const QString filename = pluginDir.path() + lit("/") + entry;

            QString libName = entry.left(entry.lastIndexOf(lit(".")));
            static const auto kPrefix = lit("lib");
            if (libName.startsWith(kPrefix))
                libName.remove(0, kPrefix.size());

            if (disabledLibNames.contains(libName))
            {
                NX_WARNING(this) << lm("Skipped loading Nx plugin [%1] (blacklisted by %2)")
                    .args(filename, pluginsIni().iniFile());
                continue;
            }

            if (!enabledLibNames.isEmpty() && !enabledLibNames.contains(libName))
            {
                NX_WARNING(this) << lm("Skipped loading Nx plugin [%1] (not whitelisted by %2)")
                    .args(filename, pluginsIni().iniFile());
                continue;
            }

            loadNxPlugin(settingsForPlugin, filename, libName);
            // Ignore return value - an error is aleady logged.
        }
    }
}

bool PluginManager::loadQtPlugin(const QString& fullFilePath)
{
    QSharedPointer<QPluginLoader> plugin(new QPluginLoader(fullFilePath));
    if(!plugin->load())
        return false;

    QObject* obj = plugin->instance();
    QnAbstractClientPlugin* clientPlugin = dynamic_cast<QnAbstractClientPlugin*>(obj);
    if (!clientPlugin)
        return false;
    clientPlugin->initializeLog(QnLog::instance().get());
    if(!clientPlugin->initialized())
    {
        NX_LOG(lit("Failed to initialize Qt plugin %1").arg(fullFilePath), cl_logERROR);
        return false;
    }

    NX_LOG(lit("Successfully loaded Qt plugin %1").arg(fullFilePath), cl_logWARNING);
    m_qtPlugins.push_back(plugin);

    emit pluginLoaded();
    return true;
}

bool PluginManager::loadNxPlugin(
    const std::vector<nxpl::Setting>& settingsForPlugin,
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

    typedef nxpl::PluginInterface* (*EntryProc)();

    auto entryProc = (EntryProc) lib.resolve("createNXPluginInstance");
    if (entryProc == nullptr)
        entryProc = (EntryProc) lib.resolve("createNxMetadataPlugin");
    if (entryProc == nullptr)
    {
        NX_ERROR(this) << lit("Failed to load Nx plugin [%1]: "
            "Neither createNXPluginInstance nor createNxMetadataPlugin functions found")
            .arg(filename);
        lib.unload();
        return false;
    }

    nxpl::PluginInterface* obj = entryProc();
    if (!obj)
    {
        NX_ERROR(this) << lit("Failed to load Nx plugin [%1]: no PluginInterface function found")
            .arg(filename);
        lib.unload();
        return false;
    }

    NX_WARNING(this) << lit("Loaded Nx plugin [%1]").arg(filename);
    m_nxPlugins.push_back(obj);
    m_libNameByNxPlugin.insert(obj, libName);

    if (auto pluginObj = nxpt::ScopedRef<nxpl::Plugin>(obj->queryInterface(nxpl::IID_Plugin)))
    {
        // Report settings to the plugin.
        if (!settingsForPlugin.empty())
            pluginObj->setSettings(&settingsForPlugin[0], (int) settingsForPlugin.size());
    }

    if (auto plugin2Obj = nxpt::ScopedRef<nxpl::Plugin2>(obj->queryInterface(nxpl::IID_Plugin2)))
    {
        if (m_pluginContainer)
            plugin2Obj->setPluginContainer(m_pluginContainer);
    }

    if (auto plugin3Obj = nxpt::ScopedRef<nxpl::Plugin3>(obj->queryInterface(nxpl::IID_Plugin3)))
    {
        plugin3Obj->setLocale("en_US"); //< TODO: Change to the real locale.
    }

    emit pluginLoaded();
    return true;
}
