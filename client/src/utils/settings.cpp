#include "settings.h"

#include <QtCore/QSettings>
#include <QtCore/QDir>

#include "licensing/license.h"
#include "utils/common/util.h"
#include "utils/common/log.h"
#include "ui/style/globals.h"

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
        QUrl url = connection.url;
        url.setPassword(QString()); /* Don't store password. */

        settings->setValue(QLatin1String("name"), connection.name);
        settings->setValue(QLatin1String("url"), url.toString());
        settings->setValue(QLatin1String("readOnly"), connection.readOnly);
    }

} // anonymous namespace


Q_GLOBAL_STATIC(QnSettings, qn_settings)

QnSettings::QnSettings():
    m_lock(QMutex::Recursive),
    m_settings(new QSettings())
{
    load();
}

QnSettings *QnSettings::instance()
{
    return qn_settings();
}

bool QnSettings::isAfterFirstRun() const
{
    return m_afterFirstRun;
}

void QnSettings::load()
{
    QMutexLocker locker(&m_lock);

    reset();

#if 0
    m_data.mediaRoot = fromNativePath(m_settings->value("mediaRoot").toString());

    int size = m_settings->beginReadArray("auxMediaRoot");
    for (int i = 0; i < size; ++i)
    {
        m_settings->setArrayIndex(i);
        m_data.auxMediaRoots.push_back(fromNativePath(m_settings->value("path").toString()));
    }
    m_settings->endArray();

    m_data.afterFirstRun = (m_settings->value("afterFirstRun").toString() == "true");

    m_data.animateBackground = m_settings->value("animateBackground", true).toBool();

#ifdef QN_HAS_BACKGROUND_COLOR_ADJUSTMENT
    m_data.backgroundColor = m_settings->value("backgroundColor", qnGlobals->backgroundGradientColor()).value<QColor>();
#else
    m_data.backgroundColor = qnGlobals->backgroundGradientColor();
#endif
    m_data.maxVideoItems = m_settings->value("maxVideoItems", 32).toInt();
    m_data.downmixAudio = m_settings->value("downmixAudio").toBool();
    m_data.openLayoutsOnLogin = m_settings->value("openLayoutsOnLogin", false).toBool();

    if (m_data.mediaRoot.isEmpty())
        m_data.mediaRoot = getMoviesDirectory() + QLatin1String("/EVE Media/");

    m_settings->beginGroup(QLatin1String("AppServerConnections"));
    m_settings->beginGroup(QLatin1String("lastUsed"));
    m_lastUsed = readConnectionData(m_settings);
    m_settings->endGroup();
    m_settings->endGroup();
    m_data.forceSoftYUV = false;
#endif
}

void QnSettings::save()
{
#if 0
    QMutexLocker locker(&m_lock);

    m_settings->setValue("mediaRoot", QDir::toNativeSeparators(m_data.mediaRoot));

    m_settings->beginWriteArray("auxMediaRoot", m_data.auxMediaRoots.size());
    int size = m_data.auxMediaRoots.size();
    for (int i = 0; i < size; ++i)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue("path", m_data.auxMediaRoots[i]);
    }
    m_settings->endArray();

    m_settings->setValue("animateBackground", m_data.animateBackground);

#ifdef QN_HAS_BACKGROUND_COLOR_ADJUSTMENT
    m_settings->setValue("backgroundColor", m_data.backgroundColor);
#endif

    m_settings->setValue("afterFirstRun", "true");
    m_settings->setValue("maxVideoItems", QString::number(m_data.maxVideoItems));
    m_settings->setValue("downmixAudio", m_data.downmixAudio ? "true" : "false");
#endif
}

bool QnSettings::layoutsOpenedOnLogin() const {
    return m_openLayoutsOnLogin;
}

void QnSettings::setLayoutsOpenedOnLogin(bool openLayoutsOnLogin) {
    //m_data.openLayoutsOnLogin = openLayoutsOnLogin;
}

int QnSettings::maxVideoItems() const {
    QMutexLocker lock(&m_lock);

    return m_maxVideoItems;
}

void QnSettings::setMaxVideoItems(int maxVideoItems) {
    QMutexLocker lock(&m_lock);

    maxVideoItems = maxVideoItems;
}



bool QnSettings::downmixAudio() const
{
    QMutexLocker _lock(&m_lock);

    return m_downmixAudio;
}

bool QnSettings::isAllowChangeIP() const
{
    QMutexLocker _lock(&m_lock);

    return m_allowChangeIP;
}

QString QnSettings::mediaRoot() const
{
    QMutexLocker _lock(&m_lock);

    return m_mediaRoot;
}

void QnSettings::setMediaRoot(const QString& root)
{
    QMutexLocker _lock(&m_lock);

    //m_data.mediaRoot = root;
}

QStringList QnSettings::auxMediaRoots() const
{
    QMutexLocker _lock(&m_lock);

    return m_auxMediaRoots;
}

void QnSettings::addAuxMediaRoot(const QString& root)
{
    QMutexLocker _lock(&m_lock);

    // Do not add duplicates
    if (m_auxMediaRoots.indexOf(root) != -1)
        return;

    m_auxMediaRoots.append(fromNativePath(root));
}

QnSettings::ConnectionData QnSettings::lastUsedConnection()
{
    QMutexLocker locker(&m_lock);

    return m_lastUsed;
}

bool QnSettings::isBackgroundAnimated() const {
    QMutexLocker locker(&m_lock);

    return m_animateBackground;
}

bool QnSettings::isForceSoftYUV() const
{
    QMutexLocker locker(&m_lock);
    return m_forceSoftYUV;
}

void QnSettings::setForceSoftYUV(bool value)
{
    QMutexLocker locker(&m_lock);
    m_forceSoftYUV = value;
}

QColor QnSettings::backgroundColor() const {
    QMutexLocker locker(&m_lock);

    return m_backgroundColor;
}

void QnSettings::setLastUsedConnection(const QnSettings::ConnectionData &connection)
{
    QMutexLocker locker(&m_lock);

    m_lastUsed = connection;

    m_settings->beginGroup(QLatin1String("AppServerConnections"));
    m_settings->beginGroup(QLatin1String("lastUsed"));
    writeConnectionData(m_settings, connection);
    m_settings->endGroup();
    m_settings->endGroup();

    emit lastUsedConnectionChanged();
}

QList<QnSettings::ConnectionData> QnSettings::connections()
{
    QMutexLocker locker(&m_lock);

    QList<ConnectionData> connections;

    const int size = m_settings->beginReadArray(QLatin1String("AppServerConnections"));
    for (int i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);
        ConnectionData connection = readConnectionData(m_settings);
        if (connection.url.isValid())
            connections.append(connection);
    }
    m_settings->endArray();

    return connections;
}

void QnSettings::setConnections(const QList<QnSettings::ConnectionData> &connections)
{
    QMutexLocker locker(&m_lock);

    ConnectionData lastUsed = lastUsedConnection();

    m_settings->beginWriteArray(QLatin1String("AppServerConnections"));
    m_settings->remove(QLatin1String("")); // clear
    int i = 0;
    foreach (const ConnectionData &connection, connections) {
        if (!connection.url.isValid())
            continue;

        if (connection.name.trimmed().isEmpty()) {
            // special case: the last used connection
            lastUsed = connection;
        } else {
            m_settings->setArrayIndex(i++);
            writeConnectionData(m_settings, connection);
        }
    }
    m_settings->endArray();

    setLastUsedConnection(lastUsed);
}

/// Private methods. No internal synchronization needed.
void QnSettings::setAllowChangeIP(bool allow)
{
    m_allowChangeIP = allow;
}

void QnSettings::removeAuxMediaRoot(const QString& root)
{
    m_auxMediaRoots.removeAll(root);
}

void QnSettings::setAuxMediaRoots(const QStringList& auxMediaRoots)
{
    m_auxMediaRoots.clear();

    foreach(const QString& auxMediaRoot, auxMediaRoots)
    {
        addAuxMediaRoot(auxMediaRoot);
    }
}

void QnSettings::reset()
{
    m_maxVideoItems = 0;
#ifdef Q_OS_DARWIN
    // mac version use SPDIF by default for multichannel audio
    m_downmixAudio = true;
#else
    m_downmixAudio = false;
#endif
    m_afterFirstRun = false;
    m_allowChangeIP = false;
}
