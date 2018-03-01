#include "plugin_manager.h"

#include <algorithm>
#include <set>

#include <QtCore/QAtomicPointer>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <nx/utils/log/log.h>

#include <decoders/abstractclientplugin.h>
#include <decoders/abstractvideodecoderplugin.h>
#include <plugins/camera_plugin.h>
#include <nx/sdk/metadata/plugin.h>

using namespace nx::sdk;

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

    //loading plugins
    if(!m_pluginDir.isEmpty())
        directoriesToSearchForPlugins.insert(QDir(m_pluginDir).absolutePath());

#ifndef Q_OS_WIN32
    char* vmsPluginDir = getenv("VMS_PLUGIN_DIR");
    if( vmsPluginDir )
        directoriesToSearchForPlugins.insert( QString::fromLatin1(vmsPluginDir) );
#endif

    //directoriesToSearchForPlugins.insert( QDir(QDir::currentPath()).absolutePath() );
    directoriesToSearchForPlugins.insert(QDir(QCoreApplication::applicationDirPath()).absolutePath()
        + lit("/plugins/"));

    //preparing settings for NX plugins
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

    for (const QString& dir: directoriesToSearchForPlugins)
    {
        loadPluginsFromDir(settingsForPlugin, dir, pluginsToLoad);
    }

    for (nxpl::Setting& setting: settingsForPlugin)
    {
        delete[] setting.name;
        delete[] setting.value;
    }
}

void PluginManager::loadPluginsFromDir(
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

        if (pluginsToLoad & QtPlugin )
            loadQtPlugin( pluginDir.path() + lit("/") + entry);

        if (pluginsToLoad & NxPlugin)
            loadNxPlugin(settingsForPlugin, pluginDir.path() + lit("/") + entry);
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
    const QString& fullFilePath)
{
    QLibrary lib(fullFilePath);
    // Flag DeepBindHint forces plugin (the loaded side) to use its functions instead of the same
    // named functions of server (the loading side). In Linux it is no so by default.
    QLibrary::LoadHints hints=lib.loadHints();
    hints |= QLibrary::DeepBindHint;
    lib.setLoadHints(hints);

    if(!lib.load())
    {
        NX_LOG(lit("Failed to load %1: %2").arg(fullFilePath).arg(lib.errorString()), cl_logERROR);
        return false;
    }

    nxpl::PluginInterface* (*entryProc)() = (decltype(entryProc))lib.
        resolve("createNXPluginInstance");

    if (entryProc == nullptr)
        entryProc = (decltype(entryProc))lib.resolve("createNxMetadataPlugin");

    if (entryProc == nullptr)
    {
        NX_LOG(lit("Failed to load %1: no createNXPluginInstance").arg(fullFilePath), cl_logERROR);
        lib.unload();
        return false;
    }

    nxpl::PluginInterface* obj = entryProc();
    if (obj == nullptr)
    {
        NX_LOG(lit("Failed to load %1: no PluginInterface").arg(fullFilePath), cl_logERROR);
        lib.unload();
        return false;
    }

    NX_LOG(lit("Successfully loaded NX plugin %1").arg(fullFilePath), cl_logWARNING);
    m_nxPlugins.push_back(obj);

    //checking whether plugin supports nxpl::Plugin interface
    nxpl::Plugin* pluginObj = static_cast<nxpl::Plugin*>(obj->queryInterface(nxpl::IID_Plugin));
    if (pluginObj)
    {
        //reporting settings to plugin
        if (!settingsForPlugin.empty())
        {
            pluginObj->setSettings(&settingsForPlugin[0],
                static_cast<int>(settingsForPlugin.size()));
        }

        pluginObj->releaseRef();
    }

    nxpl::Plugin2* plugin2Obj = static_cast<nxpl::Plugin2*>(obj->queryInterface(nxpl::IID_Plugin2));
    if (plugin2Obj)
    {
        if (m_pluginContainer)
            plugin2Obj->setPluginContainer(m_pluginContainer);

        plugin2Obj->releaseRef();
    }

    nxpl::Plugin3* plugin3Obj = static_cast<nxpl::Plugin3*>(obj->queryInterface(nxpl::IID_Plugin3));
    if (plugin3Obj)
    {
        plugin3Obj->setLocale("en_US"); //< Change to real locale.
        plugin3Obj->releaseRef();
    }

    emit pluginLoaded();
    return true;
}
