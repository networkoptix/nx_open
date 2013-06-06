#include "client_settings.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QSettings>

#include <client/config.h>

#include <utils/common/util.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/variant.h>
#include <utils/common/string.h>

#include <ui/style/globals.h>

#include <client/client_meta_types.h>

#include <version.h>


namespace {
    QnConnectionData readConnectionData(QSettings *settings)
    {
        QnConnectionData connection;
        connection.name = settings->value(QLatin1String("name")).toString();
        connection.url = settings->value(QLatin1String("url")).toString();
        connection.url.setScheme(QLatin1String("https"));
        connection.readOnly = (settings->value(QLatin1String("readOnly")).toString() == QLatin1String("true"));

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

    const QString xorKey = QLatin1String("ItIsAGoodDayToDie");

} // anonymous namespace

QnClientSettings::QnClientSettings(QObject *parent):
    base_type(parent),
    m_settings(new QSettings(this)),
    m_loading(true)
{
    init();

    /* Set default values. */
    setBackgroundColor(qnGlobals->backgroundGradientColor());
    setMediaFolder(getMoviesDirectory() + QLatin1String(QN_MEDIA_FOLDER_NAME));
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

    /* Set command line switch names. */
    addArgumentName(SOFTWARE_YUV,          "--soft-yuv");
    addArgumentName(OPEN_LAYOUTS_ON_LOGIN, "--open-layouts-on-login");
    addArgumentName(MAX_VIDEO_ITEMS,       "--max-video-items");

    /* Load from internal resource. */
    QFile file(QLatin1String(QN_SKIN_PATH) + QLatin1String("/globals.json"));
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QVariantMap json;
        if(!QJson::deserialize(file.readAll(), &json)) {
            qWarning() << "Client settings file could not be parsed!";
        } else {
            updateFromJson(json.value(lit("settings")).toMap());
        }
    }

    /* Load from settings. */
    load();

    setThreadSafe(true);

    m_loading = false;
}

QnClientSettings::~QnClientSettings() {
    return;
}

QVariant QnClientSettings::readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) {
    switch(id) {
    case DEFAULT_CONNECTION: {
        QnConnectionData result;
        result.url.setScheme(QLatin1String("https"));
        result.url.setHost(settings->value(QLatin1String("appserverHost"), QLatin1String(DEFAULT_APPSERVER_HOST)).toString());
        result.url.setPort(settings->value(QLatin1String("appserverPort"), DEFAULT_APPSERVER_PORT).toInt());
        result.url.setUserName(settings->value(QLatin1String("appserverLogin"), QLatin1String("admin")).toString());
        result.name = QLatin1String("default");
        result.readOnly = true;

        if(!result.isValid())
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
            if (connection.isValid())
                result.append(connection);
        }
        settings->endArray();

        return QVariant::fromValue<QnConnectionDataList>(result);
    }
    case AUDIO_VOLUME: {
        settings->beginGroup(QLatin1String("audioControl"));
        qreal result = qvariant_cast<qreal>(settings->value(QLatin1String("volume"), defaultValue));
        settings->endGroup();
        return result;
    }
    case UPDATE_FEED_URL:
        // TODO: #Elric need another way to handle this. Looks hacky.
        if(settings->format() == QSettings::IniFormat) {
            return base_type::readValueFromSettings(settings, id, defaultValue);
        } else {
            return defaultValue;
        }
    case STORED_PASSWORD: {
            QString result = xorDecrypt(base_type::readValueFromSettings(settings, id, defaultValue).toString(), xorKey);
            return result;
        }
    case BACKGROUND_EDITABLE:
    case DEBUG_COUNTER:
    case DEV_MODE:
        return defaultValue; /* Not to be read from settings. */
    default:
        return base_type::readValueFromSettings(settings, id, defaultValue);
        break;
    }
}

void QnClientSettings::writeValueToSettings(QSettings *settings, int id, const QVariant &value) const {
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
            if (!connection.isValid())
                continue;

            if (connection.name.trimmed().isEmpty())
                continue; /* Last used one. */

            settings->setArrayIndex(i++);
            writeConnectionData(settings, connection);
        }
        settings->endArray();

        /* Write out last used connection as it has been cleared. */
        writeValueToSettings(settings, LAST_USED_CONNECTION, QVariant::fromValue<QnConnectionData>(lastUsedConnection()));
        break;
    }
    case AUDIO_VOLUME: {
        settings->beginGroup(QLatin1String("audioControl"));
        settings->setValue(QLatin1String("volume"), value);
        settings->endGroup();
        break;
    }
    case STORED_PASSWORD: {
            base_type::writeValueToSettings(settings, id, xorEncrypt(value.toString(), xorKey));
            break;
        }
    case BACKGROUND_EDITABLE:
    case DEBUG_COUNTER:
    case UPDATE_FEED_URL:
    case DEV_MODE:
        break; /* Not to be saved to settings. */
    default:
        base_type::writeValueToSettings(settings, id, value);
        break;
    }
}

void QnClientSettings::updateValuesFromSettings(QSettings *settings, const QList<int> &ids) {
    QnScopedValueRollback<bool> guard(&m_loading, true);

    base_type::updateValuesFromSettings(settings, ids);
}

QnPropertyStorage::UpdateStatus QnClientSettings::updateValue(int id, const QVariant &value) {
    UpdateStatus status = base_type::updateValue(id, value);

    /* Settings are to be written out right away. */
    if(!m_loading && status == Changed)
        writeValueToSettings(m_settings, id, this->value(id));

    return status;
}

void QnClientSettings::load() {
    updateFromSettings(m_settings);
}

void QnClientSettings::save() {
    submitToSettings(m_settings);
}

bool QnClientSettings::isWritable() const {
    return m_settings->isWritable();
}
