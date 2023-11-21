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
#include <nx/vms/api/data/watermark_settings.h>

namespace nx::vms::api {

/**
 * This structure should only be used to save system settings. It repeats `SystemSettings`, but all
 * fields are wrapped with `std::optional`, so we can serialize only a changed portion of it.
 */
struct SaveableSystemSettings
{
    std::optional<QString> systemName;
    std::optional<bool> autoDiscoveryEnabled;
    std::optional<bool> cameraSettingsOptimization;
    std::optional<bool> statisticsAllowed;
    std::optional<QString> cloudNotificationsLanguage;
    std::optional<EmailSettings> emailSettings;

    std::optional<bool> timeSynchronizationEnabled;
    std::optional<QnUuid> primaryTimeServer;

    std::optional<WatermarkSettings> watermarkSettings;
    std::optional<bool> auditTrailEnabled;
    std::optional<bool> trafficEncryptionForced;
    std::optional<bool> useHttpsOnlyForCameras;
    std::optional<bool> videoTrafficEncryptionForced;
    std::optional<std::chrono::seconds> sessionLimitS;
    std::optional<bool> storageEncryption;
    std::optional<bool> showServersInTreeForNonAdmins;

    std::optional<bool> updateNotificationsEnabled;
    std::optional<nx::utils::Url> customReleaseListUrl;
    std::optional<ClientUpdateSettings> clientUpdateSettings;

    std::optional<BackupSettings> backupSettings;
    std::optional<MetadataStorageChangePolicy> metadataStorageChangePolicy;
};

NX_REFLECTION_INSTRUMENT(SaveableSystemSettings,
    (systemName)(autoDiscoveryEnabled)(cameraSettingsOptimization)(statisticsAllowed)
    (cloudNotificationsLanguage)(emailSettings)(timeSynchronizationEnabled)(primaryTimeServer)
    (watermarkSettings)(auditTrailEnabled)(trafficEncryptionForced)(useHttpsOnlyForCameras)
    (videoTrafficEncryptionForced)(sessionLimitS)(storageEncryption)(showServersInTreeForNonAdmins)
    (updateNotificationsEnabled)(customReleaseListUrl)(clientUpdateSettings)
    (backupSettings)(metadataStorageChangePolicy))

NX_REFLECTION_TAG_TYPE(SaveableSystemSettings, jsonSerializeChronoDurationAsNumber)

} // namespace nx::vms::api
