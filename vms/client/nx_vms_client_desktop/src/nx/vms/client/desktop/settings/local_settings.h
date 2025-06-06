// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QStringList>

#include <common/common_globals.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/property_storage/storage.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/core/settings/types/detected_object.h>
#include <nx/vms/client/core/system_logon/connection_data.h>
#include <nx/vms/client/desktop/analytics/attribute_display_manager.h>
#include <nx/vms/client/desktop/event_search/right_panel_globals.h>
#include <nx/vms/client/desktop/export/settings/export_layout_persistent_settings.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/vms/client/desktop/jsapi/auth_allowed_urls.h>
#include <nx/vms/client/desktop/webpage/web_page_settings.h>
#include <nx/vms/common/system_health/message_type.h>
#include <ui/workbench/handlers/workbench_screenshot_handler.h>

#include "types/background_image.h"
#include "types/export_mode.h"

namespace nx::vms::client::desktop {

class LocalSettings: public nx::utils::property_storage::Storage
{
    Q_OBJECT

public:
    LocalSettings();

    void reload();

    bool hardwareDecodingEnabled();

    Q_INVOKABLE static QVariant iniConfigValue(const QString& name);

    Property<int> maxSceneVideoItems{this, "maxSceneVideoItems", 64,
        "Maximum number of items that is allowed on the scene."};
    Property<int> maxPreviewSearchItems{this, "maxPreviewSearchItems", 16,
        "Maximum number of items that is allowed during the preview search."};
    Property<int> tourCycleTimeMs{this, "tourCycleTimeMs", 4000};

    Property<bool> createFullCrashDump{this, "createFullCrashDump"};
    Property<int> statisticsNetworkFilter{this, "statisticsNetworkFilter", 1,
        "Filter value for network connections in the statistics widget."};
    Property<bool> glDoubleBuffer{this, "glDoubleBuffer", true,
        "Allow double buffering for OpenGL context."};
    Property<bool> glBlurEnabled{this, "glBlurEnabled", true, "Allow blur on video items."};
    Property<bool> autoFpsLimit{this, "autoFpsLimit", false, "Automatically limit FPS."};
    Property<bool> showFisheyeCalibrationGrid{this, "showFisheyeCalibrationGrid"};
    Property<bool> hardwareDecodingEnabledProperty{this, "hardwareDecodingEnabled", false,
        "Whether hardware video decoding is used, if available."};
    Property<int> maxIntelHardwareDecoders{this, "maxIntelHardwareDecoders", 8,
        "Maximum number of hardware decoders for Intel QS."};
    Property<int> maxNVidiaHardwareDecoders{ this, "maxNVidiaHardwareDecoders", 16,
        "Maximum number of hardware decoders for NVidia." };
    Property<int> maxHardwareDecodingStreams{ this, "maxHardwareDecodingStreams", 16,
        "Maximum number of hardware decoders." };

    Property<QString> logLevel{this, "logLevel", "info"};
    Property<qint64> maxLogFileSizeB{this, nx::log::kMaxLogFileSizeSymbolicName,
        nx::log::kDefaultMaxLogFileSizeB};
    Property<qint64> maxLogVolumeSizeB{this, nx::log::kMaxLogVolumeSizeSymbolicName,
        nx::log::kDefaultMaxLogVolumeSizeB};
    Property<std::chrono::seconds> maxLogFileTimePeriodS{this,
        nx::log::kMaxLogFileTimePeriodSymbolicName,
        nx::log::kDefaultMaxLogFileTimePeriodS};
    Property<bool> logArchivingEnabled{this, nx::log::kLogArchivingEnabledSymbolicName,
        nx::log::kDefaultLogArchivingEnabled};

    Property<nx::Uuid> pcUuid{this, "pcUuid", {},
        "Unique id for this PC for videowall construction."};

    Property<Qn::ResourceInfoLevel> resourceInfoLevel{this, "resourceInfoLevel", Qn::RI_NameOnly,
        "Level of resource detalization in the resource tree, notifications, etc. Includes "
            "displaying camera IP addresses, user roles, videowall layouts."};
    Property<bool> showFullInfo{this, "showFullInfo", false,
        "When 'Info' mode is enabled on cameras, full info will be displayed even without hover."};
    Property<bool> showInfoByDefault{this, "showInfoByDefault", false,
        "'Info' mode will be enabled by default on newly opened cameras."};

    Property<Qn::TimeMode> timeMode{this, "timeMode", Qn::ClientTimeMode};

    Property<bool> ptzAimOverlayEnabled{this, "ptzAimOverlayEnabled", false,
        "Whether old-style PTZ aim overlay is enabled."};

    Property<bool> autoStart{this, "autoStart"};
    Property<bool> allowComputerEnteringSleepMode{this, "allowComputerEnteringSleepMode", false,
        "Allow the computer to enter sleep mode on idle while running the Client."};

    Property<int> acceptedEulaVersion{this, "acceptedEulaVersion", 0,
        "Version of the latest read and accepted EULA."};

    Property<bool> downmixAudio{this,
        "downmixAudio",
        nx::build_info::isMacOsX()}; //< Mac version uses SPDIF by default for multichannel audio.
    Property<qreal> audioVolume{this, "audioVolume", 1.0};

    Property<bool> playAudioForAllItems{this, "playAudioForAllItems", false,
        "Play audio for all items at the same time if true."};

    Property<quint64> userIdleTimeoutMs{this, "userIdleTimeoutMs"};

    Property<BackgroundImage> backgroundImage{
        this, "backgroundImage", BackgroundImage()};

    Property<core::ConnectionData> lastUsedConnection{this, "lastUsedConnection"};
    Property<QString> lastLocalConnectionUrl{this, "lastLocalConnectionUrl", {},
        "Last site we successfully connected to."};
    Property<bool> autoLogin{this, "autoLogin", false, "Auto-login to the last connected server."};
    Property<bool> saveCredentialsAllowed{this, "saveCredentialsAllowed", true};
    Property<bool> stickReconnectToServer{this, "stickReconnectToServer", false,
        "Force client to reconnect only to the same server it was connected to."};
    Property<bool> restoreUserSessionData{this, "restoreUserSessionData", true,
        "Enables restoration of the session-specific data."};
    Property<bool> layoutKeepAspectRatio{this, "layoutKeepAspectRatio", true,
        "Last used value for the 'Keep aspect ratio' flag in the layout settings."};

    Property<ExportMediaPersistentSettings> exportMediaSettings{this, "exportMediaSettings"};
    Property<ExportLayoutPersistentSettings> exportLayoutSettings{this, "exportLayoutSettings"};
    Property<ExportMediaPersistentSettings> exportBookmarkSettings{this, "exportBookmarkSettings",
        ExportMediaPersistentSettings({ExportOverlayType::bookmark})};
    Property<ExportMode> lastExportMode{this, "lastExportMode", ExportMode::media};

    Property<QString> lastDatabaseBackupDir{this, "lastDatabaseBackupDir"};
    Property<QString> lastDownloadDir{this, "lastDownloadDir"};
    Property<QString> lastExportDir{this, "lastExportDir"};
    Property<QString> lastImportDir{this, "lastImportDir"};
    Property<QString> lastRecordingDir{this, "lastRecordingDir"};
    Property<SharedScreenshotParameters> lastScreenshotParams{this, "lastScreenshotParams"};
    Property<QString> lastCreatedUserLocale{this, "lastCreatedUserLocale"};

    Property<QString> backgroundsFolder{this, "backgroundsFolder", {},
        "Last used path for the layout backgrounds."};
    Property<QStringList> mediaFolders{this, "mediaFolders"};

    Property<bool> allowMtDecoding{this, "allowMtDecoding", true,
        "Disable MT decoding if false, enable auto selection if true."};
    Property<bool> forceMtDecoding{this, "forceMtDecoding", false,
        "Always force MT decoding if true."};

    Property<bool> browseLogsButtonVisible{this, "browseLogsButtonVisible", true,
        "[Support] Whether 'Browse Logs' button is visible on Advanced page of Local Settings."};

    Property<int> initialLiveBufferMs{this, "initialLiveBufferMs", 50,
        "Initial live buffer lengths, in milliseconds."};
    Property<int> maximumLiveBufferMs{this, "maximumLiveBufferMs", 500,
        "Maximum live buffer lengths, in milliseconds."};

    Property<core::DetectedObjectSettingsMap> detectedObjectSettings{this, "detectedObjectSettings"};

    Property<AuthAllowedUrls> authAllowedUrls{this, "authAllowedUrls", {},
        "Approved URLs that have access to a session token using jsapi."};

    Property<int> maxMp3FileDurationSec{this, "maxMp3FileDurationSec", 30,
        "Maximum file duration in seconds for sound notification actions."};

    Property<QMap<QString, WebPageSettings>> webPageSettings{this, "webPageSettings", {},
        "Web Page settings."};

    Property<bool> showTimestampOnLiveCamera{this, "showTimestampOnLiveCamera", false,
        "Show timestamp on live cameras (instead of text \"LIVE\")."};

    struct AnalyticsAttributeSettings
    {
        QString id;
        bool visible = false;
        bool operator==(const AnalyticsAttributeSettings& other) const = default;
    };
    Property<QList<AnalyticsAttributeSettings>> analyticsSearchTileVisibleAttributes{this,
        "analyticsSearchTileVisibleAttributes",
        {},
        "List and order of visible attributes in the tile view of the Advanced Search Dialog."};
    Property<QList<AnalyticsAttributeSettings>> analyticsSearchTableVisibleAttributes{this,
        "analyticsSearchTableVisibleAttributes",
        {},
        "List and order of visible attributes in the table view of the Advanced Search Dialog."};

    void migrateOldSettings();

private:
    // Deprecated properties, moved to the client core settings and kept here only for the
    // migration reasons.
    Property<QString> locale{this, "locale", nx::branding::defaultLocale()};

    Property<bool> muteOnAudioTransmit{this, "muteOnAudioTransmit", true,
        "Mute audio output while two way audio is engaged."};

private:
    void migrateSettingsFrom5_1(LocalSettings* settings, QSettings* oldSettings);
    void migrateFrom6_0(LocalSettings* settings);
    void setDefaults();
};

NX_REFLECTION_INSTRUMENT(LocalSettings::AnalyticsAttributeSettings, (id)(visible))

} // namespace nx::vms::client::desktop
