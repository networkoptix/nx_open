// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <string>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(SiteHealthMessageType,
    // These messages are generated on the client.

    emailIsEmpty = 0, //< Current user email is empty.
    noLicenses = 1,
    smtpIsNotSet = 2,
    usersEmailIsEmpty = 3, //< Other user's email is empty.

    // These messages are sent from server.

    emailSendError = 7,
    storagesNotConfigured = 8, //< Generated on the client side since 4.1 hotfix.
    archiveRebuildFinished = 10,
    archiveRebuildCanceled = 11,
    archiveFastScanFinished = 12,

    cloudPromo = 13, //< Promo message. Generated on the client side.

    remoteArchiveSyncError = 17, //< Generated on the server side.
    archiveIntegrityFailed = 18, //< Generated on the server side.

    defaultCameraPasswords = 19, //< Generated on the client side.
    noInternetForTimeSync = 20, //< Generated on the client side.
    backupStoragesNotConfigured = 21, //< Generated on the client side since 4.3.

    cameraRecordingScheduleIsInvalid = 22, //< Generated on the client side.
    replacedDeviceDiscovered = 23, //< Camera discovered is currently replaced by the given one.

    metadataStorageNotSet = 27, //< Generated on the client side.
    metadataOnSystemStorage = 28, //< Generated on the client side.

    saasLocalRecordingServicesOverused = 29, //< Generated on the client side.
    saasCloudStorageServicesOverused = 30, //< Generated on the client side.
    saasIntegrationServicesOverused = 31, //< Generated on the client side.

    saasInSuspendedState = 32, //< Generated on the client side.
    saasInShutdownState = 33, //< Generated on the client side.
    /**
     * Open an intercom as an action on notification press.
     *
     * actionParams:
     * - actionResourceId - intercom camera id
     */
    showIntercomInformer = 34,

    /**
     * Show an intercom missed call notification. It replaces the opened showIntercomInformer's
     * notification with the same actionResourceId.
     *
     * actionParams:
     * - actionResourceId - intercom camera id
     */
    showMissedCallInformer = 35,

    /**
     * Show a warning that current user notification language differs from the client application
     * language. Generated on the client side.
     */
    notificationLanguageDiffers = 37,

    /**
     * Show a warning that current Saas Tier is overused. Generated on the client side.
     */
    saasTierIssue = 38,

    /**
     * Show a warning that service was disabled automatically due to SaaS issue
     */
    recordingServiceDisabled = 39,
    cloudServiceDisabled = 40,
    integrationServiceDisabled = 41,

    cloudStorageIsAvailable = 42, //< Generated on the client side
    cloudStorageIsEnabled = 43, //< Generated on the client side

    count
)

struct SiteHealthMessage
{
    SiteHealthMessageType type = SiteHealthMessageType::count;
    bool active = true;
    std::chrono::microseconds timestampUs = std::chrono::microseconds::zero();
    UuidList resourceIds;
    std::map<std::string, std::string> attributes;

    nx::Uuid resourceId() const { return resourceIds.empty() ? nx::Uuid() : resourceIds.front(); }
};

#define SiteHealthMessage_Fields (type)(timestampUs)(resourceIds)(attributes)
NX_REFLECTION_INSTRUMENT(SiteHealthMessage, SiteHealthMessage_Fields)

} // namespace nx::vms::api
