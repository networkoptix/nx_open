#include "settings.h"

#include <QtCore/QSettings>

#include <utils/common/util.h>
#include <utils/common/scoped_value_rollback.h>

#include <ui/style/globals.h>

namespace {
    QnConnectionData readConnectionData(QSettings *settings)
    {
        QnConnectionData connection;
        connection.name = settings->value(QLatin1String("name")).toString();
        connection.url = settings->value(QLatin1String("url")).toString();
        connection.readOnly = (settings->value(QLatin1String("readOnly")).toString() == "true");

        return connection;
    }

    void writeConnectionData(QSettings *settings, const QnConnectionData &connection)
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
    m_mutex(QMutex::Recursive),
    m_settings(new QSettings(this)),
    m_loading(true)
{
    init();

    /* Set default values. */
    setBackgroundColor(qnGlobals->backgroundGradientColor());
    setMediaFolder(getMoviesDirectory() + QLatin1String("/HD Witness Media/"));
#ifdef Q_OS_DARWIN
    setAudioDownmixed(true); /* Mac version uses SPDIF by default for multichannel audio. */
#endif

    /* Set names. */
    setName(MEDIA_FOLDER,           QLatin1String("mediaRoot"));
    setName(EXTRA_MEDIA_FOLDERS,    QLatin1String("auxMediaRoot"));
    setName(BACKGROUND_ANIMATED,    QLatin1String("animateBackground"));
    setName(BACKGROUND_COLOR,       QLatin1String("backgroundColor"));
    setName(MAX_VIDEO_ITEMS,        QLatin1String("maxVideoItems"));
    setName(DOWNMIX_AUDIO,          QLatin1String("downmixAudio"));
    setName(OPEN_LAYOUTS_ON_LOGIN,  QLatin1String("openLayoutsOnLogin"));

    connect(notifier(LAST_USED_CONNECTION), SIGNAL(valueChanged(int)), this, SIGNAL(lastUsedConnectionChanged()));

    /* Load from settings. */
    load();

    m_loading = false;
}

QnSettings::~QnSettings() {
    return;
}

QnSettings *QnSettings::instance() {
    return qn_settings();
}

void QnSettings::lock() const {
    m_mutex.lock();
}

void QnSettings::unlock() const {
    m_mutex.unlock();
}

QVariant QnSettings::updateValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) {
    switch(id) {
    case DEFAULT_CONNECTION: {
        QnConnectionData result;
        result.url.setScheme(QLatin1String("https"));
        result.url.setHost(settings->value("appserverHost", QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
        result.url.setPort(settings->value("appserverPort", DEFAULT_APPSERVER_PORT).toInt());
        result.url.setUserName(settings->value("appserverLogin", QLatin1String("admin")).toString());
        result.name = QLatin1String("default");
        result.readOnly = true;

        if(!result.url.isValid())
            result.url = QUrl(QString(QLatin1String("https://admin@%1:%2")).arg(QLatin1String(DEFAULT_APPSERVER_HOST)).arg(DEFAULT_APPSERVER_PORT));

        return QVariant::fromValue<QnConnectionData>(result);
    }
    case LAST_USED_CONNECTION: {
        settings->beginGroup(QLatin1String("AppServerConnections"));
        settings->beginGroup(QLatin1String("lastUsed"));
        QnConnectionData result = readConnectionData(settings);
        settings->endGroup();
        settings->endGroup();
        return QVariant::fromValue<QnConnectionData>(result);
    }
    case CUSTOM_CONNECTIONS: {
        QnConnectionDataList result;

        const int size = settings->beginReadArray(QLatin1String("AppServerConnections"));
        for (int i = 0; i < size; ++i) {
            settings->setArrayIndex(i);
            QnConnectionData connection = readConnectionData(settings);
            if (connection.url.isValid()) {
                connection.url.setScheme("https");
                result.append(connection);
            }
        }
        settings->endArray();

        return QVariant::fromValue<QnConnectionDataList>(result);
    }
#ifndef QN_HAS_BACKGROUND_COLOR_ADJUSTMENT
    case BACKGROUND_COLOR:
        return qnGlobals->backgroundGradientColor();
#endif
    default:
        return base_type::updateValueFromSettings(settings, id, defaultValue);
        break;
    }
}

void QnSettings::submitValueToSettings(QSettings *settings, int id, const QVariant &value) const {
    switch(id) {
    case LAST_USED_CONNECTION:
        settings->beginGroup(QLatin1String("AppServerConnections"));
        settings->beginGroup(QLatin1String("lastUsed"));
        writeConnectionData(settings, value.value<QnConnectionData>());
        settings->endGroup();
        settings->endGroup();
        break;
    case CUSTOM_CONNECTIONS: {
        settings->beginWriteArray(QLatin1String("AppServerConnections"));
        settings->remove(QLatin1String("")); /* Clear children. */
        int i = 0;
        foreach (const QnConnectionData &connection, value.value<QnConnectionDataList>()) {
            if (!connection.url.isValid())
                continue;

            if (connection.name.trimmed().isEmpty())
                continue; /* Last used one. */

            settings->setArrayIndex(i++);
            writeConnectionData(settings, connection);
        }
        settings->endArray();

        /* Write out last used connection as it has been cleared. */
        submitValueToSettings(settings, LAST_USED_CONNECTION, QVariant::fromValue<QnConnectionData>(lastUsedConnection()));
        break;
    }
#ifndef QN_HAS_BACKGROUND_COLOR_ADJUSTMENT
    case BACKGROUND_COLOR:
        break;
#endif
    default:
        base_type::submitValueToSettings(settings, id, value);
        break;
    }
}

void QnSettings::updateFromSettings(QSettings *settings) {
    QMutexLocker locker(&m_mutex);
    QnScopedValueRollback<bool> guard(&m_loading, true);

    base_type::updateFromSettings(settings);
}

bool QnSettings::updateValue(int id, const QVariant &value) {
    bool changed = base_type::updateValue(id, value);

    /* Settings are to be written out right away. */
    if(!m_loading && changed)
        submitValueToSettings(m_settings, id, this->value(id));

    return changed;
}

void QnSettings::load() {
    updateFromSettings(m_settings);
}

void QnSettings::save() {
    submitToSettings(m_settings);
}

