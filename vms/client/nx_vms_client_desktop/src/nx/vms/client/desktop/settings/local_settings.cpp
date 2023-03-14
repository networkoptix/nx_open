// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_settings.h"

#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/media/hardware_acceleration_type.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/json.h>
#include <nx/utils/file_system.h>
#include <nx/utils/log/log.h>
#include <nx/utils/property_storage/filesystem_backend.h>
#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

namespace {

QString getDefaultBackgroundsFolder()
{
    if (!nx::build_info::isWindows())
    {
        return nx::format("/opt/%1/client/share/pictures/sample-backgrounds",
            nx::branding::companyId());
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
    baseDir.cd(nx::format("%1 Backgrounds", nx::branding::vmsName()));
    return baseDir.absolutePath();
}

QString getDefaultMediaFolder()
{
    const QStringList locations = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
    if (locations.isEmpty())
        return {};

    return locations.first() + "/" + nx::branding::mediaFolderName();
}

void migratePopupSystemHealthFrom5_1(LocalSettings* settings, QSettings* oldSettings)
{
    if (settings->popupSystemHealth.exists())
        return;

    if (!oldSettings->contains("popupSystemHealth"))
        return;

    const quint64 intValue = oldSettings->value("popupSystemHealth").toULongLong();

    using MessageType = common::system_health::MessageType;

    const auto skipPackedFlag =
        [](MessageType message)
        {
            switch (message)
            {
                // These were skipped in 3.0.
                case MessageType::cloudPromo:

                case MessageType::archiveFastScanFinished:
                // TODO: Remove in VMS-7724.
                case MessageType::remoteArchiveSyncError:
                    return true;

                default:
                    return false;
            }
        };

    std::set<MessageType> result;
    quint64 healthFlag = 1;
    for (int i = 0; i < (int) MessageType::count; ++i)
    {
        const auto messageType = (MessageType) i;
        if (skipPackedFlag(messageType))
            continue;

        if (isMessageVisibleInSettings(messageType))
        {
            if ((intValue & healthFlag) == healthFlag)
                result.insert(messageType);
        }

        healthFlag <<= 1;
    }

    settings->popupSystemHealth = result;
}

void migrateSettingsFrom5_1(LocalSettings* settings, QSettings* oldSettings)
{
    const auto migrateValue =
        [oldSettings]<typename T>(
            nx::utils::property_storage::Property<T>& property, const QString& customName = {})
        {
            if (property.exists())
                return;

            auto name = customName.isNull() ? property.name : customName;
            if (oldSettings->contains(name))
                property = oldSettings->value(name).template value<T>();
        };

    const auto migrateJsonValue =
        [oldSettings]<typename T>(
            nx::utils::property_storage::Property<T>& property, const QString& customName = {})
        {
            if (property.exists())
                return;

            auto name = customName.isNull() ? property.name : customName;
            if (!oldSettings->contains(name))
                return;

            const auto serializedValue = oldSettings->value(name).toString();
            const auto [value, result] = nx::reflect::json::deserialize<T>(
                serializedValue.toStdString());

            if (result)
            {
                property = value;
            }
            else
            {
                NX_WARNING(NX_SCOPE_TAG,
                    "Cannot deserialize JSON value from old settings: %1: %2. Error: %3",
                    name, serializedValue, result.errorDescription);
            }
        };

        const auto migrateEnumValue =
        [oldSettings]<typename T>(
            nx::utils::property_storage::Property<T>& property, const QString& customName = {})
        {
            if (property.exists())
                return;

            auto name = customName.isNull() ? property.name : customName;
            if (!oldSettings->contains(name))
                return;

            const auto serializedValue = oldSettings->value(name).toString();
            bool ok = false;
            const T value = nx::reflect::fromString<T>(serializedValue.toStdString(), &ok);

            if (ok)
            {
                property = value;
            }
            else
            {
                NX_WARNING(NX_SCOPE_TAG, "Cannot deserialize enum value from old settings: %1: %2",
                    name, serializedValue);
            }
        };

    migrateValue(settings->maxSceneVideoItems);
    migrateValue(settings->maxPreviewSearchItems);
    migrateValue(settings->tourCycleTimeMs, "tourCycleTime");
    migrateValue(settings->createFullCrashDump);
    migrateValue(settings->statisticsNetworkFilter);
    migrateValue(settings->glDoubleBuffer, "isGlDoubleBuffer");
    migrateValue(settings->glBlurEnabled, "isGlBlurEnabled");
    migrateValue(settings->autoFpsLimit, "isAutoFpsLimit");
    migrateValue(settings->showFisheyeCalibrationGrid, "isFisheyeCalibrationGridShown");
    migrateValue(settings->hardwareDecodingEnabledProperty, "isHardwareDecodingEnabledProperty");
    migrateValue(settings->maxHardwareDecoders);
    migrateValue(settings->locale);
    migrateValue(settings->logLevel);
    migrateValue(settings->pcUuid);
    migrateEnumValue(settings->resourceInfoLevel);
    migrateValue(settings->showFullInfo);
    migrateValue(settings->showInfoByDefault);
    migrateValue(settings->timeMode);
    migrateValue(settings->ptzAimOverlayEnabled, "isPtzAimOverlayEnabled");
    migrateValue(settings->autoStart);
    migrateValue(settings->allowComputerEnteringSleepMode);
    migrateValue(settings->acceptedEulaVersion);
    migrateValue(settings->downmixAudio);
    migrateValue(settings->audioVolume, "audioControl/volume");
    migrateValue(settings->muteOnAudioTransmit);
    migrateValue(settings->playAudioForAllItems);
    migrateValue(settings->userIdleTimeoutMs, "userIdleTimeoutMSecs");
    migrateJsonValue(settings->backgroundImage);
    migrateJsonValue(settings->lastUsedConnection);
    migrateValue(settings->lastLocalConnectionUrl);
    migrateValue(settings->autoLogin);
    migrateValue(settings->saveCredentialsAllowed);
    migrateValue(settings->stickReconnectToServer);
    migrateValue(settings->restoreUserSessionData);
    migrateJsonValue(settings->exportMediaSettings);
    migrateJsonValue(settings->exportLayoutSettings);
    migrateJsonValue(settings->exportBookmarkSettings);
    migrateEnumValue(settings->lastExportMode);
    migrateValue(settings->lastDatabaseBackupDir);
    migrateValue(settings->lastDownloadDir);
    migrateValue(settings->lastExportDir, "export/previousDir");
    migrateValue(settings->lastRecordingDir, "videoRecording/previousDir");
    migrateValue(settings->lastScreenshotDir);
    migrateValue(settings->backgroundsFolder);
    migrateValue(settings->mediaFolders);
    migrateValue(settings->allowMtDecoding);
    migrateValue(settings->forceMtDecoding);
    migrateValue(settings->browseLogsButtonVisible, "isBrowseLogsVisible");
    migrateValue(settings->initialLiveBufferMs);
    migrateValue(settings->maximumLiveBufferMs);
    migrateJsonValue(settings->detectedObjectSettings);
    migrateJsonValue(settings->authAllowedUrls);
    migrateValue(settings->maxMp3FileDurationSec);

    migratePopupSystemHealthFrom5_1(settings, oldSettings);
}

void migrateMediaFoldersFrom4_3(LocalSettings* settings, QSettings* oldSettings)
{
    if (settings->mediaFolders.exists())
        return;

    QStringList mediaFolders;

    const QVariant mediaRoot = oldSettings->value("mediaRoot");
    if (!mediaRoot.isNull())
        mediaFolders.append(mediaRoot.toString());

    const QVariant auxMediaRoot = oldSettings->value("auxMediaRoot");
    if (!auxMediaRoot.isNull())
        mediaFolders += auxMediaRoot.toStringList();

    settings->mediaFolders = mediaFolders;
}

void migrateLastUsedConnectionFrom4_2(LocalSettings* settings, QSettings* oldSettings)
{
    if (settings->lastUsedConnection.exists())
        return;

    core::ConnectionData data;
    data.url = oldSettings->value("AppServerConnections/lastUsed/url").toString();
    data.url.setScheme(nx::network::http::kSecureUrlSchemeName);
    data.systemId = oldSettings->value("localId").toUuid();

    settings->lastUsedConnection = data;
}

} // namespace

LocalSettings::LocalSettings():
    Storage(new nx::utils::property_storage::FileSystemBackend(
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/local"))
{
    load();

    migrateOldSettings();
    setDefaults();

    if (!mediaFolders().isEmpty())
        nx::utils::file_system::ensureDir(mediaFolders().first());
}

void LocalSettings::reload()
{
    load();
}

bool LocalSettings::hardwareDecodingEnabled()
{
    static auto hardwareAccelerationType = nx::media::getHardwareAccelerationType();
    if (!ini().nvidiaHardwareDecoding
        && hardwareAccelerationType == nx::media::HardwareAccelerationType::nvidia)
    {
        hardwareAccelerationType = nx::media::HardwareAccelerationType::none;
    }

    if (hardwareAccelerationType == nx::media::HardwareAccelerationType::none)
        return false;

    return hardwareDecodingEnabledProperty();
}

QVariant LocalSettings::iniConfigValue(const QString& name)
{
    return ini().get(name);
}

void LocalSettings::migrateOldSettings()
{
    const auto oldSettings = std::make_unique<QSettings>();

    migrateSettingsFrom5_1(this, oldSettings.get());
    migrateLastUsedConnectionFrom4_2(this, oldSettings.get());
    migrateMediaFoldersFrom4_3(this, oldSettings.get());
}

void LocalSettings::setDefaults()
{
    maxSceneVideoItems = (sizeof(void*) == sizeof(qint32)) ? 24 : 64;

    if (nx::build_info::isMacOsX())
        downmixAudio = true; //< Mac version uses SPDIF by default for multichannel audio.

    if (backgroundsFolder().isEmpty())
        backgroundsFolder = getDefaultBackgroundsFolder();

    if (mediaFolders().isEmpty())
        mediaFolders = {getDefaultMediaFolder()};
}

} // namespace nx::vms::client::desktop
