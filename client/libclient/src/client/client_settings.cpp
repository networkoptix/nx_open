#include "client_settings.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QSettings>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <QtCore/QJsonDocument>

#include <utils/common/util.h>
#include <nx/fusion/serialization/json_functions.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/variant.h>
#include <nx/utils/string.h>

#include <ui/style/globals.h>

#include <client/client_meta_types.h>
#include <client/client_runtime_settings.h>

#include <utils/common/app_info.h>
#include <utils/common/warnings.h>

namespace
{
static const QString kXorKey = lit("ItIsAGoodDayToDie");

static const auto kNameTag = lit("name");
static const auto kUrlTag = lit("url");
static const auto kIsCustomTag = lit("isCustom");
static const auto kPasswordTag = lit("pwd");

QnConnectionData readConnectionData(QSettings *settings)
{
    QnConnectionData connection;

    const bool useHttps = settings->value(lit("secureAppserverConnection"), true).toBool();
    connection.url = settings->value(kUrlTag).toString();
    connection.url.setScheme(useHttps ? lit("https") : lit("http"));
    connection.name = settings->value(kNameTag).toString();
    connection.isCustom = settings->value(kIsCustomTag, true).toBool();

    const auto password = settings->value(kPasswordTag).toString();
    if (!password.isEmpty())
        connection.url.setPassword(nx::utils::xorDecrypt(password, kXorKey));

    return connection;
}

void writeConnectionData(QSettings *settings, const QnConnectionData &connection)
{
    QUrl url = connection.url;

    QString password;
    if (!url.password().isEmpty())
        nx::utils::xorEncrypt(url.password(), kXorKey);

    url.setPassword(QString()); /* Don't store password in plain text. */

    settings->setValue(kNameTag, connection.name);
    settings->setValue(kIsCustomTag, connection.isCustom);
    settings->setValue(kPasswordTag, password);
    settings->setValue(kUrlTag, url.toString());
}

} // anonymous namespace
QnClientSettings::QnClientSettings(bool forceLocalSettings, QObject *parent):
    base_type(parent),
    m_loading(true)
{
    if (forceLocalSettings)
        m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, qApp->organizationName(), qApp->applicationName(), this);
    else
        m_settings = new QSettings(this);

    init();

    /* Set default values. */
    setMediaFolder(getMoviesDirectory());
    setBackgroundsFolder(getBackgroundsDirectory());
    setMaxSceneVideoItems(sizeof(void *) == sizeof(qint32) ? 24 : 64);

#ifdef Q_OS_DARWIN
    setAudioDownmixed(true); /* Mac version uses SPDIF by default for multichannel audio. */
#endif
    setShowcaseUrl(QUrl(QnAppInfo::showcaseUrl()));
    setSettingsUrl(QUrl(QnAppInfo::settingsUrl()));

    /* Set names (compatibility with 1.0). */
    setName(MEDIA_FOLDER,           lit("mediaRoot"));
    setName(EXTRA_MEDIA_FOLDERS,    lit("auxMediaRoot"));
    setName(DOWNMIX_AUDIO,          lit("downmixAudio"));
    setName(LAST_RECORDING_DIR,     lit("videoRecording/previousDir"));
    setName(LAST_EXPORT_DIR,        lit("export/previousDir"));

    /* Load from internal resource. */
    QFile file(QLatin1String(":/globals.json"));
    if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonObject jsonObject;
        if(!QJson::deserialize(file.readAll(), &jsonObject)) {
            NX_ASSERT(false, Q_FUNC_INFO, "Client settings file could not be parsed!");
        } else {
            updateFromJson(jsonObject.value(lit("settings")).toObject());
        }
    }

    /* Load from settings. */
    load();

    /* Update showcase url from external source. */
    NX_ASSERT(!isShowcaseEnabled(), Q_FUNC_INFO, "Paxton dll crashes here, make sure showcase fucntionality is disabled");
    if (isShowcaseEnabled())
        loadFromWebsite();

    setThreadSafe(true);

    m_loading = false;
}

QnClientSettings::~QnClientSettings() {
}

QVariant QnClientSettings::readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) {
    switch(id)
    {
        case LAST_USED_CONNECTION:
        {
            settings->beginGroup(QLatin1String("AppServerConnections"));
            settings->beginGroup(QLatin1String("lastUsed"));
            auto result = readConnectionData(settings);
            settings->endGroup();
            settings->endGroup();
            return QVariant::fromValue<QnConnectionData>(result);
        }
        case CUSTOM_CONNECTIONS:
        {
            QnConnectionDataList result;
            const int size = settings->beginReadArray(QLatin1String("AppServerConnections"));
            for (int index = 0; index < size; ++index)
            {
                settings->setArrayIndex(index);
                QnConnectionData connection = readConnectionData(settings);
                if (connection.isValid())
                    result.append(connection);
            }
            settings->endArray();

            return QVariant::fromValue<QnConnectionDataList>(result);
        }
        case AUDIO_VOLUME:
        {
            settings->beginGroup(QLatin1String("audioControl"));
            qreal result = qvariant_cast<qreal>(settings->value(QLatin1String("volume"), defaultValue));
            settings->endGroup();
            return result;
        }
        case LIGHT_MODE:
        {
            QVariant baseValue = base_type::readValueFromSettings(settings, id, defaultValue);
            if (baseValue.type() == QVariant::Int)  //compatibility mode
                return qVariantFromValue(static_cast<Qn::LightModeFlags>(baseValue.toInt()));
            return baseValue;
        }


        case SHOW_ONCE_MESSAGES:
        {
            QVariant baseValue = base_type::readValueFromSettings(settings, id, defaultValue);
            return qVariantFromValue(static_cast<Qn::ShowOnceMessages>(baseValue.toInt()));
        }

        case WORKBENCH_PANES:
        {
            QByteArray asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toUtf8();
            return QVariant::fromValue(QJson::deserialized<QnPaneSettingsMap>(asJson,
                defaultValue.value<QnPaneSettingsMap>()));
        }

        case BACKGROUND_IMAGE:
        {
            QByteArray asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toUtf8();
            return QVariant::fromValue(QJson::deserialized<QnBackgroundImage>(asJson,
                defaultValue.value<QnBackgroundImage>()));
        }

        default:
            return base_type::readValueFromSettings(settings, id, defaultValue);
            break;
    }
}

void QnClientSettings::writeValueToSettings(QSettings *settings, int id, const QVariant &value) const {
    if (qnRuntime->isVideoWallMode() || qnRuntime->isActiveXMode())
        return;

    switch(id)
    {
        case LAST_USED_CONNECTION:
            settings->beginGroup(QLatin1String("AppServerConnections"));
            settings->beginGroup(QLatin1String("lastUsed"));
            writeConnectionData(settings, value.value<QnConnectionData>());
            settings->endGroup();
            settings->endGroup();
            break;
        case CUSTOM_CONNECTIONS:
        {
            settings->beginWriteArray(QLatin1String("AppServerConnections"));
            settings->remove(QLatin1String("")); /* Clear children. */

            int index = 0;
            for (const auto &connection: value.value<QnConnectionDataList>())
            {
                if (!connection.isValid())
                    continue;

                if (connection.name.trimmed().isEmpty())
                    continue; /* Last used one. */

                settings->setArrayIndex(index++);
                writeConnectionData(settings, connection);
            }
            settings->endArray();

            /* Write out last used connection as it has been cleared. */
            writeValueToSettings(settings, LAST_USED_CONNECTION,
                QVariant::fromValue<QnConnectionData>(lastUsedConnection()));
            break;
        }
        case AUDIO_VOLUME:
        {
            settings->beginGroup(QLatin1String("audioControl"));
            settings->setValue(QLatin1String("volume"), value);
            settings->endGroup();
            break;
        }

        case SHOW_ONCE_MESSAGES:
            base_type::writeValueToSettings(settings, id, static_cast<int>(value.value<Qn::ShowOnceMessages>()));

        case UPDATE_FEED_URL:
        case SETTINGS_URL:
        case GL_VSYNC:
        case LIGHT_MODE:
        case NO_CLIENT_UPDATE:
            break; /* Not to be saved to settings. */

        case WORKBENCH_PANES:
        {
            QString asJson = QString::fromUtf8(QJson::serialized(value.value<QnPaneSettingsMap>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        case BACKGROUND_IMAGE:
        {
            QString asJson = QString::fromUtf8(QJson::serialized(value.value<QnBackgroundImage>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        default:
            base_type::writeValueToSettings(settings, id, value);
            break;
    }
}

void QnClientSettings::updateValuesFromSettings(QSettings *settings, const QList<int> &ids) {
    QN_SCOPED_VALUE_ROLLBACK(&m_loading, true);

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
    m_settings->sync();
}

bool QnClientSettings::isWritable() const {
    return m_settings->isWritable();
}

QSettings* QnClientSettings::rawSettings() {
    return m_settings;
}

void QnClientSettings::loadFromWebsite() {
    QNetworkAccessManager *accessManager = new QNetworkAccessManager(this);
    connect(accessManager, &QNetworkAccessManager::finished, this, [accessManager, this](QNetworkReply *reply) {
        if(reply->error() != QNetworkReply::NoError) {
            qnWarning("Could not download client settings from '%1': %2.", reply->url().toString(), reply->errorString());
        } else {
            QJsonObject jsonObject;
            if(!QJson::deserialize(reply->readAll(), &jsonObject)) {
                qnWarning("Could not parse client settings downloaded from '%1'.", reply->url().toString());
            } else {
                updateFromJson(jsonObject.value(lit("settings")).toObject());
            }
        }
        reply->deleteLater();
        accessManager->deleteLater();
    });
    accessManager->get(QNetworkRequest(settingsUrl()));
}
