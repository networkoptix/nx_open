// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_settings.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QStandardPaths>

#include <client_core/client_core_settings.h>

#include <client/client_meta_types.h>

#include <utils/common/util.h>
#include <utils/common/variant.h>
#include <utils/mac_utils.h>

#include <nx/network/http/http_types.h>

#include <nx/fusion/model_functions.h>

#include <nx/reflect/json.h>

#include <nx/utils/file_system.h>
#include <nx/utils/string.h>
#include <nx/utils/url.h>
#include <nx/vms/client/desktop/ini.h>

#include <nx/branding.h>
#include <nx/build_info.h>

#include "client_startup_parameters.h"

using namespace nx::vms::client::desktop;

namespace {

using CertificateValidationLevel =
    nx::vms::client::core::network::server_certificate::ValidationLevel;

QString getMoviesDirectory()
{
    QString result;
#ifdef Q_OS_MACX
    result = mac_getMoviesDir();
#else
    const auto moviesDirs = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
    result = moviesDirs[0];
#endif

    return result.isEmpty()
        ? QString()
        : result + "/" + nx::branding::mediaFolderName();
}

QString getBackgroundsDirectory()
{
    if (!nx::build_info::isWindows())
    {
        return QString("/opt/%1/client/share/pictures/sample-backgrounds")
            .arg(nx::branding::companyId());
    }

    const auto picturesLocations = QStandardPaths::standardLocations(
        QStandardPaths::PicturesLocation);
    const auto documentsLocations = QStandardPaths::standardLocations(
        QStandardPaths::DocumentsLocation);

    QDir baseDir;
    if (!picturesLocations.isEmpty())
        baseDir.setPath(picturesLocations.first());
    else if (!documentsLocations.isEmpty())
        baseDir.setPath(documentsLocations.first());
    baseDir.cd(QString("%1 Backgrounds").arg(nx::branding::vmsName()));
    return baseDir.absolutePath();
}

} // namespace

template<> QnClientSettings* Singleton<QnClientSettings>::s_instance = nullptr;

QnClientSettings::QnClientSettings(const QnStartupParameters& startupParameters, QObject* parent):
    base_type(parent),
    m_readOnly(startupParameters.isVideoWallMode() || startupParameters.acsMode)
{
    if (startupParameters.forceLocalSettings)
    {
        m_settings = new QSettings(
            QSettings::IniFormat,
            QSettings::UserScope,
            qApp->organizationName(),
            qApp->applicationName(),
            this);
    }
    else
    {
        m_settings = new QSettings(this);
    }

    init();

    /* Set default values. */
    setMediaFolders(QStringList{getMoviesDirectory()});
    setBackgroundsFolder(getBackgroundsDirectory());
    setMaxSceneVideoItems(sizeof(void *) == sizeof(qint32) ? 24 : 64);

#ifdef Q_OS_DARWIN
    setAudioDownmixed(true); /* Mac version uses SPDIF by default for multichannel audio. */
#endif

    /* Set names (compatibility with 1.0). */
    setName(DOWNMIX_AUDIO,          lit("downmixAudio"));
    setName(LAST_RECORDING_DIR,     lit("videoRecording/previousDir"));
    setName(LAST_EXPORT_DIR,        lit("export/previousDir"));

    /* Load from settings. */
    load();

    if (!mediaFolders().isEmpty())
        nx::utils::file_system::ensureDir(mediaFolders().first());

    setThreadSafe(true);

    if (!m_settings->isWritable())
    {
        auto fileName = m_settings->fileName();
        if (!fileName.isEmpty())
        {
            NX_ERROR(this, "QnClientSettings() - client can not write settings to %1. "
                "Please check file permissions.", fileName);
        }
        else
        {
            NX_ERROR(this, "QnClientSettings() - client can not write settings.");
        }
    }

    m_loading = false;
}

QnClientSettings::~QnClientSettings() {
}

QVariant QnClientSettings::readValueFromSettings(QSettings* settings, int id,
    const QVariant& defaultValue) const
{
    switch(id)
    {
        case LAST_USED_CONNECTION:
        {
            std::string asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toStdString();
            nx::vms::client::core::ConnectionData result;
            if (nx::reflect::json::deserialize(asJson, &result))
                return QVariant::fromValue(result);
            return defaultValue;
        }
        case AUDIO_VOLUME:
        {
            settings->beginGroup(QLatin1String("audioControl"));
            qreal result = qvariant_cast<qreal>(settings->value(QLatin1String("volume"), defaultValue));
            settings->endGroup();
            return result;
        }
        case WORKBENCH_PANES:
        {
            QByteArray asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toUtf8();
            return QVariant::fromValue(QJson::deserialized<QnPaneSettingsMap>(asJson,
                defaultValue.value<QnPaneSettingsMap>()));
        }

        case WORKBENCH_STATES:
        {
            QByteArray asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toUtf8();
            return QVariant::fromValue(QJson::deserialized<QnWorkbenchStateList>(asJson,
                defaultValue.value<QnWorkbenchStateList>()));
        }

        case EXPORT_MEDIA_SETTINGS:
        case EXPORT_BOOKMARK_SETTINGS:
        {
            const auto asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toStdString();
            auto [settings, error] = nx::reflect::json::deserialize<ExportMediaSettings>(asJson);
            return QVariant::fromValue(error.success
                ? settings
                : ExportMediaSettings());
        }

        case EXPORT_LAYOUT_SETTINGS:
        {
            const auto asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toStdString();
            auto [settings, error] = nx::reflect::json::deserialize<ExportLayoutSettings>(asJson);
            return QVariant::fromValue(error.success
                ? settings
                : ExportLayoutSettings());
        }

        case BACKGROUND_IMAGE:
        {
            QByteArray asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toUtf8();
            return QVariant::fromValue(QJson::deserialized<QnBackgroundImage>(asJson,
                defaultValue.value<QnBackgroundImage>()));
        }

        case RESOURCE_INFO_LEVEL:
        {
            const QVariant value = base_type::readValueFromSettings(settings, id, QVariant());
            const auto defaultLevel = defaultValue.value<Qn::ResourceInfoLevel>();

            if (!value.isValid())
            {
                // Compatibility mode with 4.2 and lower: read old key, containing quoted int value.
                // Explicit check is the most safe and simple.
                const QString compatibilityValue = settings->value("extraInfoInTree").toString();
                if (compatibilityValue == "\"1\"")
                    return QVariant::fromValue(Qn::RI_NameOnly);
                if (compatibilityValue == "\"3\"")
                    return QVariant::fromValue(Qn::RI_FullInfo);

                return QVariant::fromValue(defaultLevel);
            }

            return QVariant::fromValue(
                nx::reflect::fromString(value.toString().toStdString(), defaultLevel));
        }

        case POPUP_SYSTEM_HEALTH:
        {
            quint64 packed = base_type::readValueFromSettings(settings, id, 0xFFFFFFFFFFFFFFFFull)
                .toULongLong();
            return QVariant::fromValue(QnSystemHealth::unpackVisibleInSettings(packed));
        }

        case UPDATE_DELIVERY_INFO:
        {
            QByteArray asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toUtf8();
            return QVariant::fromValue(QJson::deserialized<UpdateDeliveryInfo>(asJson,
                defaultValue.value<UpdateDeliveryInfo>()));
        }

        case AUTH_ALLOWED_URLS:
        {
            auto asJson = base_type::readValueFromSettings(settings, id, QVariant())
                .value<QString>().toStdString();

            auto [urls, result] = nx::reflect::json::deserialize<AuthAllowedUrls>(asJson);

            return result ? QVariant::fromValue(urls) : defaultValue;
        }

        default:
            return base_type::readValueFromSettings(settings, id, defaultValue);
            break;
    }
}

void QnClientSettings::writeValueToSettings(QSettings *settings, int id, const QVariant &value) const {
    if (m_readOnly)
        return;

    switch(id)
    {
        case LAST_USED_CONNECTION:
        {
            QString asJson = QString::fromStdString(
                nx::reflect::json::serialize(value.value<nx::vms::client::core::ConnectionData>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }
        case AUDIO_VOLUME:
        {
            settings->beginGroup(QLatin1String("audioControl"));
            settings->setValue(QLatin1String("volume"), value);
            settings->endGroup();
            break;
        }

        case WORKBENCH_PANES:
        {
            QString asJson = QString::fromUtf8(QJson::serialized(value.value<QnPaneSettingsMap>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        case WORKBENCH_STATES:
        {
            QString asJson = QString::fromUtf8(QJson::serialized(value.value<QnWorkbenchStateList>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        case BACKGROUND_IMAGE:
        {
            QString asJson = QString::fromUtf8(QJson::serialized(value.value<QnBackgroundImage>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        case EXPORT_MEDIA_SETTINGS:
        case EXPORT_BOOKMARK_SETTINGS:
        {
            const auto asJson = QString::fromStdString(
                nx::reflect::json::serialize(value.value<ExportMediaSettings>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        case EXPORT_LAYOUT_SETTINGS:
        {
            const auto asJson = QString::fromStdString(
                nx::reflect::json::serialize(value.value<ExportLayoutSettings>()));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        case RESOURCE_INFO_LEVEL:
        {
            const auto level = value.value<Qn::ResourceInfoLevel>();
            base_type::writeValueToSettings(
                settings, id, QString::fromStdString(nx::reflect::toString(level)));
            break;
        }

        case POPUP_SYSTEM_HEALTH:
        {
            quint64 packed = base_type::readValueFromSettings(settings, id, 0xFFFFFFFFFFFFFFFFull)
                .toULongLong();
            packed = QnSystemHealth::packVisibleInSettings(packed,
                value.value<QSet<QnSystemHealth::MessageType>>());
            base_type::writeValueToSettings(settings, id, packed);
            break;
        }

        case UPDATE_DELIVERY_INFO:
        {
            const auto& updateInfo = value.value<UpdateDeliveryInfo>();
            QString asJson = QString::fromUtf8(QJson::serialized(updateInfo));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        case AUTH_ALLOWED_URLS:
        {
            const auto urls = value.value<AuthAllowedUrls>();
            QString asJson = QString::fromStdString(nx::reflect::json::serialize(urls));
            base_type::writeValueToSettings(settings, id, asJson);
            break;
        }

        default:
            base_type::writeValueToSettings(settings, id, value);
            break;
    }
}

void QnClientSettings::updateValuesFromSettings(QSettings *settings, const QList<int> &ids) {
    QScopedValueRollback<bool> guard(m_loading, true);

    base_type::updateValuesFromSettings(settings, ids);
}

QnPropertyStorage::UpdateStatus QnClientSettings::updateValue(int id, const QVariant &value) {
    UpdateStatus status = base_type::updateValue(id, value);

    /* Settings are to be written out right away. */
    if(!m_loading && status == Changed)
        writeValueToSettings(m_settings, id, this->value(id));

    return status;
}

void QnClientSettings::migrateLastUsedConnection()
{
    // Skip anything if migration already occurred.
    if (base_type::readValueFromSettings(m_settings, LAST_USED_CONNECTION, QVariant()).isValid())
        return;

    nx::vms::client::core::ConnectionData data;
    m_settings->beginGroup("AppServerConnections");
    m_settings->beginGroup("lastUsed");
    data.url = m_settings->value("url").toString();
    data.url.setScheme(nx::network::http::kSecureUrlSchemeName);
    data.systemId = m_settings->value("localId").toUuid();
    m_settings->endGroup();
    m_settings->endGroup();

    writeValueToSettings(m_settings, LAST_USED_CONNECTION, QVariant::fromValue(data));
    setLastUsedConnection(data);
}

void QnClientSettings::migrateMediaFolders()
{
    if (m_settings->contains(name(MEDIA_FOLDERS)))
        return;

    QVariant mediaFolderSetting = m_settings->value("mediaRoot");
    QVariant extraMediaFoldersSetting = m_settings->value("auxMediaRoot");

    if (mediaFolderSetting.isNull() && extraMediaFoldersSetting.isNull())
        return;

    QStringList mediaFolders;

    if (!mediaFolderSetting.isNull())
        mediaFolders.append(mediaFolderSetting.toString());

    if (!extraMediaFoldersSetting.isNull())
        mediaFolders += extraMediaFoldersSetting.toStringList();

    writeValueToSettings(m_settings, MEDIA_FOLDERS, QVariant::fromValue(mediaFolders));
    setMediaFolders(mediaFolders);
}

void QnClientSettings::load()
{
    m_settings->sync();
    updateFromSettings(m_settings);
}

bool QnClientSettings::save()
{
    submitToSettings(m_settings);
    if (!isWritable())
        return false;

    m_settings->sync();
    emit saved();
    return true;
}

bool QnClientSettings::isWritable() const
{
    return m_settings->isWritable();
}

QSettings* QnClientSettings::rawSettings()
{
    return m_settings;
}

void QnClientSettings::migrate()
{
    migrateLastUsedConnection();
    migrateMediaFolders();
}

QVariant QnClientSettings::iniConfigValue(const QString& paramName)
{
    return nx::vms::client::desktop::ini().get(paramName);
}
