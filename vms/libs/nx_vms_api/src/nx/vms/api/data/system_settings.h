// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/reflect/instrument.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/api/data/client_update_settings.h>
#include <nx/vms/api/data/email_settings.h>

#include "watermark_settings.h"

namespace nx::vms::api {

struct SystemSettings
{
    QString cloudAccountName;
    QString cloudSystemID;
    QString defaultExportVideoCodec;
    QString localSystemId;
    QString systemName;
    WatermarkSettings watermarkSettings;
    bool webSocketEnabled = true;

    bool autoDiscoveryEnabled = true;
    bool cameraSettingsOptimization = true;
    bool statisticsAllowed = true;
    QString cloudNotificationsLanguage;

    bool auditTrailEnabled = true;
    bool trafficEncryptionForced = true;
    bool useHttpsOnlyForCameras = false;
    bool videoTrafficEncryptionForced = false;
    std::optional<std::chrono::seconds> sessionLimitS =
        std::optional<std::chrono::seconds>(30 * 24 * 60 * 60);
    bool storageEncryption = false;
    bool showServersInTreeForNonAdmins = true;

    bool updateNotificationsEnabled = true;

    EmailSettings emailSettings;
    bool useCloudServiceToSendEmail = false;

    bool timeSynchronizationEnabled = true;
    QnUuid primaryTimeServer;
    nx::utils::Url customReleaseListUrl;

    ClientUpdateSettings clientUpdateSettings;
    BackupSettings backupSettings;
    MetadataStorageChangePolicy metadataStorageChangePolicy = MetadataStorageChangePolicy::keep;

    bool operator==(const SystemSettings& other) const = default;
};

NX_REFLECTION_INSTRUMENT(SystemSettings, (cloudAccountName)(cloudSystemID)(defaultExportVideoCodec)
    (localSystemId)(systemName)(watermarkSettings)(webSocketEnabled)(autoDiscoveryEnabled)
    (cameraSettingsOptimization)(statisticsAllowed)(cloudNotificationsLanguage)
    (auditTrailEnabled)(trafficEncryptionForced)(useHttpsOnlyForCameras)
    (videoTrafficEncryptionForced)(sessionLimitS)(storageEncryption)
    (showServersInTreeForNonAdmins)(updateNotificationsEnabled)(emailSettings)
    (useCloudServiceToSendEmail)(timeSynchronizationEnabled)(primaryTimeServer)
    (customReleaseListUrl)(clientUpdateSettings)(backupSettings)(metadataStorageChangePolicy));

NX_REFLECTION_TAG_TYPE(SystemSettings, jsonSerializeChronoDurationAsNumber)

} // namespace nx::vms::api
