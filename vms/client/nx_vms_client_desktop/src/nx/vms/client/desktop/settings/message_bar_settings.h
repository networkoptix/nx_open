// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/property_storage/storage.h>

namespace nx::vms::client::desktop {

class MessageBarSettings: public nx::utils::property_storage::Storage
{
public:
    MessageBarSettings();

    void reload();
    void reset();

    Property<bool> detailsLoggingWarning{this, "detailsLoggingWarning", true};
    Property<bool> performanceLoggingWarning{this, "performanceLoggingWarning", true};
    Property<bool> httpsOnlyBarInfo{this, "httpsOnlyBarInfo", true};
    Property<bool> highArchiveLengthWarning{this, "highArchiveLengthWarning", true};
    Property<bool> highPreRecordingValueWarning{this, "highPreRecordingValueWarning", true};
    Property<bool> advancedSettingsAlert{this, "advancedSettingsAlert", true};
    Property<bool> highResolutionInfo{this, "highResolutionInfo", true};
    Property<bool> motionImplicitlyDisabledAlert{this, "motionImplicitlyDisabledAlert", true};
    Property<bool> motionRecordingAlert{this, "motionRecordingAlert", true};
    Property<bool> transcodingExportWarning{this, "transcodingExportWarning", true};
    Property<bool> aviWithAudioExportInfo{this, "aviWithAudioExportInfo", true};
    Property<bool> downscalingExportInfo{this, "downscalingExportInfo", true};
    Property<bool> tooLongExportWarning{this, "tooLongExportWarning", true};
    Property<bool> tooBigExeFileWarning{this, "tooBigExeFile", true};
    Property<bool> transcodingInLayoutIsNotSupportedWarning{
        this, "transcodingInLayoutIsNotSupportedWarning", true};
    Property<bool> nonCameraResourcesWarning{this, "nonCameraResourcesWarning", true};
    Property<bool> webPageInformation{this, "webPageInformation", true};
    Property<bool> integrationWarning{this, "integrationWarning", true};
    Property<bool> storageConfigAnalyticsOnSystemStorageWarning{
        this, "storageConfigAnalyticsOnSystemStorageWarning", true};
    Property<bool> storageConfigAnalyticsOnDisabledStorageWarning{
        this, "storageConfigAnalyticsOnDisabledStorageWarning", true};
    Property<bool> storageConfigHasDisabledWarning{this, "storageConfigHasDisabledWarning", true};
    Property<bool> storageConfigUsbWarning{this, "storageConfigUsbWarning", true};
    Property<bool> storageConfigCloudStorageWarning{this, "storageConfigCloudStorageWarning", true};
    Property<bool> notificationsDeprecationInfo{this, "notificationsDeprecationInfo", true};
    Property<bool> multiServerUpdateWeekendWarning{this, "multiServerUpdateWeekendWarning", true};
    Property<bool> multiServerUpdateCustomClientWarning{
        this, "multiServerUpdateCustomClientWarning", true};
};

MessageBarSettings* messageBarSettings();

} // namespace nx::vms::client::desktop
