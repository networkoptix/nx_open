#include "settings.h"

#include <QMutex>
#include <QSettings>
#include <QDir>

#include "licensing/license.h"
#include "utils/common/util.h"
#include "utils/common/log.h"

namespace {
    QnSettings::ConnectionData readConnectionData(QSettings *settings)
    {
        QnSettings::ConnectionData connection;
        connection.name = settings->value(QLatin1String("name")).toString();
        connection.url = settings->value(QLatin1String("url")).toString();
        connection.readOnly = (settings->value(QLatin1String("readOnly")).toString() == "true");

        return connection;
    }

    void writeConnectionData(QSettings *settings, const QnSettings::ConnectionData &connection)
    {
        settings->setValue(QLatin1String("name"), connection.name);
        settings->setValue(QLatin1String("url"), connection.url.toString());
        settings->setValue(QLatin1String("readOnly"), connection.readOnly);
    }

} // anonymous namespace


Q_GLOBAL_STATIC(QnSettings, qn_settings)

QnSettings::QnSettings():
    m_lock(QMutex::Recursive)
{}

QnSettings *QnSettings::instance()
{
    return qn_settings();
}

bool QnSettings::isAfterFirstRun() const
{
    return m_data.afterFirstRun;
}

void QnSettings::fillData(QnSettings::Data &data) const
{
    QMutexLocker locker(&m_lock);

    data = m_data;
}

void QnSettings::update(const QnSettings::Data &data)
{
    QMutexLocker locker(&m_lock);

    setMediaRoot(data.mediaRoot);
    setAuxMediaRoots(data.auxMediaRoots);
    setAllowChangeIP(data.allowChangeIP);
    m_data.maxVideoItems = data.maxVideoItems;
    m_data.downmixAudio = data.downmixAudio;
}

void QnSettings::load()
{
    QMutexLocker locker(&m_lock);

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

void QnSettings::save()
{
    QMutexLocker locker(&m_lock);

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

int QnSettings::maxVideoItems() const
{
    QMutexLocker _lock(&m_lock);

    return m_data.maxVideoItems;
}

bool QnSettings::downmixAudio() const
{
    QMutexLocker _lock(&m_lock);

    return m_data.downmixAudio;
}

bool QnSettings::isAllowChangeIP() const
{
    QMutexLocker _lock(&m_lock);

    return m_data.allowChangeIP;
}

QString QnSettings::mediaRoot() const
{
    QMutexLocker _lock(&m_lock);

    return m_data.mediaRoot;
}

void QnSettings::setMediaRoot(const QString& root)
{
    QMutexLocker _lock(&m_lock);

    m_data.mediaRoot = root;
}

QStringList QnSettings::auxMediaRoots() const
{
    QMutexLocker _lock(&m_lock);

    return m_data.auxMediaRoots;
}

void QnSettings::addAuxMediaRoot(const QString& root)
{
    QMutexLocker _lock(&m_lock);

    // Do not add duplicates
    if (m_data.auxMediaRoots.indexOf(root) != -1)
        return;

    m_data.auxMediaRoots.append(fromNativePath(root));
}

QnSettings::ConnectionData QnSettings::lastUsedConnection()
{
    QMutexLocker locker(&m_lock);

    ConnectionData connection;

    QSettings settings;
    settings.beginGroup(QLatin1String("AppServerConnections"));
    settings.beginGroup(QLatin1String("lastUsed"));
    connection = readConnectionData(&settings);
    settings.endGroup();
    settings.endGroup();

    return connection;
}

void QnSettings::setLastUsedConnection(const QnSettings::ConnectionData &connection)
{
    QMutexLocker locker(&m_lock);

    QSettings settings;
    settings.beginGroup(QLatin1String("AppServerConnections"));
    settings.beginGroup(QLatin1String("lastUsed"));
    writeConnectionData(&settings, connection);
    settings.endGroup();
    settings.endGroup();

    emit lastUsedConnectionChanged();
}

QList<QnSettings::ConnectionData> QnSettings::connections()
{
    QMutexLocker locker(&m_lock);

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

void QnSettings::setConnections(const QList<QnSettings::ConnectionData> &connections)
{
    QMutexLocker locker(&m_lock);

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

bool QnSettings::haveValidLicense()
{
    return qnLicensePool->haveValidLicense();
}

/// Private methods. No internal synchronization needed.
void QnSettings::setAllowChangeIP(bool allow)
{
    m_data.allowChangeIP = allow;
}

void QnSettings::removeAuxMediaRoot(const QString& root)
{
    m_data.auxMediaRoots.removeAll(root);
}

void QnSettings::setAuxMediaRoots(const QStringList& auxMediaRoots)
{
    m_data.auxMediaRoots.clear();

    foreach(const QString& auxMediaRoot, auxMediaRoots)
    {
        addAuxMediaRoot(auxMediaRoot);
    }
}

void QnSettings::reset()
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
