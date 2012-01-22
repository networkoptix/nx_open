#include "settings.h"

#include <QtCore/QMutex>
#include <QtCore/QSettings>

#include "license.h"
#include "utils/common/util.h"
#include "utils/common/log.h"

Settings::Settings()
    : m_RWLock(QReadWriteLock::Recursive)
{
}

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

bool Settings::isAfterFirstRun() const
{
    return m_data.afterFirstRun;
}

void Settings::fillData(Settings::Data& data) const
{
    QReadLocker _lock(&m_RWLock);

    data = m_data;
}

void Settings::update(const Settings::Data& data)
{
    QWriteLocker _lock(&m_RWLock);

    setMediaRoot(data.mediaRoot);
    setAuxMediaRoots(data.auxMediaRoots);
    setAllowChangeIP(data.allowChangeIP);
    m_data.maxVideoItems = data.maxVideoItems;
    m_data.downmixAudio = data.downmixAudio;
}

void Settings::load()
{
    QWriteLocker _lock(&m_RWLock);

    reset();

    m_data.mediaRoot = fromNativePath(m_settings.value("mediaRoot").toString());

    int size = m_settings.beginReadArray("auxMediaRoot");
    for (int i = 0; i < size; ++i)
    {
        m_settings.setArrayIndex(i);
        m_data.auxMediaRoots.push_back(fromNativePath(m_settings.value("path").toString()));
    }
    m_settings.endArray();

    m_data.afterFirstRun = (m_settings.value("afterFirstRun").toString() == "true");
    m_data.maxVideoItems = m_settings.value("maxVideoItems", 32).toInt();
    m_data.downmixAudio = (m_settings.value("downmixAudio") == "true");

    if (m_data.mediaRoot.isEmpty())
        m_data.mediaRoot = getMoviesDirectory() + QLatin1String("/EVE Media/");
}

void Settings::save()
{
    QWriteLocker _lock(&m_RWLock);

    m_settings.setValue("mediaRoot", QDir::toNativeSeparators(m_data.mediaRoot));

    m_settings.beginWriteArray("auxMediaRoot", m_data.auxMediaRoots.size());
    int size = m_data.auxMediaRoots.size();
    for (int i = 0; i < size; ++i)
    {
        m_settings.setArrayIndex(i);
        m_settings.setValue("path", m_data.auxMediaRoots[i]);
    }
    m_settings.endArray();

    m_settings.setValue("afterFirstRun", "true");
    m_settings.setValue("maxVideoItems", QString::number(m_data.maxVideoItems));
    m_settings.setValue("downmixAudio", m_data.downmixAudio ? "true" : "false");
}

int Settings::maxVideoItems() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.maxVideoItems;
}

bool Settings::downmixAudio() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.downmixAudio;
}

bool Settings::isAllowChangeIP() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.allowChangeIP;
}

QString Settings::mediaRoot() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.mediaRoot;
}

void Settings::setMediaRoot(const QString& root)
{
    QWriteLocker _lock(&m_RWLock);

    m_data.mediaRoot = root;
}

QStringList Settings::auxMediaRoots() const
{
    QReadLocker _lock(&m_RWLock);

    return m_data.auxMediaRoots;
}

void Settings::addAuxMediaRoot(const QString& root)
{
    QWriteLocker _lock(&m_RWLock);

    // Do not add duplicates
    if (m_data.auxMediaRoots.indexOf(root) != -1)
        return;

    m_data.auxMediaRoots.append(fromNativePath(root));
}

Q_GLOBAL_STATIC_WITH_ARGS(QMutex, globalSettingsMutex, (QMutex::Recursive))

static inline Settings::ConnectionData readConnectionData(QSettings *settings)
{
    Settings::ConnectionData connection;
    connection.name = settings->value(QLatin1String("name")).toString();
    connection.url = settings->value(QLatin1String("url")).toString();
    connection.readOnly = (settings->value(QLatin1String("readOnly")).toString() == "true");

    return connection;
}

static inline void writeConnectionData(QSettings *settings, const Settings::ConnectionData &connection)
{
    settings->setValue(QLatin1String("name"), connection.name);
    settings->setValue(QLatin1String("url"), connection.url.toString());
    settings->setValue(QLatin1String("readOnly"), connection.readOnly);
}

Settings::ConnectionData Settings::lastUsedConnection()
{
//    QMutexLocker locker(globalSettingsMutex());

    ConnectionData connection;

    QSettings settings;
    settings.beginGroup(QLatin1String("AppServerConnections"));
    settings.beginGroup(QLatin1String("lastUsed"));
    connection = readConnectionData(&settings);
    settings.endGroup();
    settings.endGroup();

    return connection;
}

void Settings::setLastUsedConnection(const Settings::ConnectionData &connection)
{
//    QMutexLocker locker(globalSettingsMutex());

    QSettings settings;
    settings.beginGroup(QLatin1String("AppServerConnections"));
    settings.beginGroup(QLatin1String("lastUsed"));
    writeConnectionData(&settings, connection);
    settings.endGroup();
    settings.endGroup();
}

QList<Settings::ConnectionData> Settings::connections()
{
//    QMutexLocker locker(globalSettingsMutex());

    QList<ConnectionData> connections;

    QSettings settings;
    const int size = settings.beginReadArray(QLatin1String("AppServerConnections"));
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        ConnectionData connection = readConnectionData(&settings);
        if (connection.url.isValid())
            connections.append(connection);
    }
    settings.endArray();

    return connections;
}

void Settings::setConnections(const QList<Settings::ConnectionData> &connections)
{
//    QMutexLocker locker(globalSettingsMutex());

    ConnectionData lastUsed = lastUsedConnection();

    QSettings settings;
    settings.beginWriteArray(QLatin1String("AppServerConnections"));
    settings.remove(QLatin1String("")); // clear
    int i = 0;
    foreach (const ConnectionData &connection, connections) {
        if (!connection.url.isValid())
            continue;

        if (connection.name.trimmed().isEmpty()) {
            // special case: the last used connection
            lastUsed = connection;
        } else {
            settings.setArrayIndex(i++);
            writeConnectionData(&settings, connection);
        }
    }
    settings.endArray();

    setLastUsedConnection(lastUsed);
}

bool Settings::haveValidLicense()
{
    return QnLicense::defaultLicense().isValid();
}

/// Private methods. No internal synchronization needed.
void Settings::setAllowChangeIP(bool allow)
{
    m_data.allowChangeIP = allow;
}

void Settings::removeAuxMediaRoot(const QString& root)
{
    m_data.auxMediaRoots.removeAll(root);
}

void Settings::setAuxMediaRoots(const QStringList& auxMediaRoots)
{
    m_data.auxMediaRoots.clear();

    foreach(const QString& auxMediaRoot, auxMediaRoots)
    {
        addAuxMediaRoot(auxMediaRoot);
    }
}

void Settings::reset()
{
    m_data.maxVideoItems = 0;
#ifdef Q_OS_DARWIN
    // mac version use SPDIF by default for multichannel audio
    m_data.downmixAudio = true;
#else
    m_data.downmixAudio = false;
#endif
    m_data.afterFirstRun = false;
    m_data.allowChangeIP = false;
}
