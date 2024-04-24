// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/proxy_connection_access_policy.h>

#include "backup_settings.h"
#include "client_update_settings.h"
#include "email_settings.h"
#include "persistent_update_storage.h"
#include "pixelation_settings.h"
#include "watermark_settings.h"

namespace nx::vms::api {

struct SaveableSystemSettings
{
    std::optional<QString> defaultExportVideoCodec;
    std::optional<QString> systemName;
    std::optional<WatermarkSettings> watermarkSettings;
    std::optional<PixelationSettings> pixelationSettings;
    std::optional<bool> webSocketEnabled;

    std::optional<bool> autoDiscoveryEnabled;
    std::optional<bool> cameraSettingsOptimization;
    std::optional<bool> statisticsAllowed;
    std::optional<QString> cloudNotificationsLanguage;

    std::optional<bool> auditTrailEnabled;
    std::optional<bool> trafficEncryptionForced;
    std::optional<bool> useHttpsOnlyForCameras;
    std::optional<bool> videoTrafficEncryptionForced;
    std::optional<std::chrono::seconds> sessionLimitS;
    std::optional<bool> useSessionLimitForCloud;
    std::optional<bool> storageEncryption;
    std::optional<bool> showServersInTreeForNonAdmins;

    std::optional<bool> updateNotificationsEnabled;

    std::optional<EmailSettings> emailSettings;

    std::optional<bool> timeSynchronizationEnabled;
    std::optional<nx::Uuid> primaryTimeServer;
    std::optional<nx::utils::Url> customReleaseListUrl;

    std::optional<ClientUpdateSettings> clientUpdateSettings;
    std::optional<BackupSettings> backupSettings;
    std::optional<MetadataStorageChangePolicy> metadataStorageChangePolicy;

    std::optional<bool> allowRegisteringIntegrations;

    std::optional<QString> additionalLocalFsTypes;
    std::optional<bool> arecontRtspEnabled;
    std::optional</*std::chrono::days*/ int> auditTrailPeriodDays;
    std::optional<bool> autoDiscoveryResponseEnabled;
    std::optional<bool> autoUpdateThumbnails;
    std::optional<std::chrono::milliseconds> checkVideoStreamPeriodMs;
    std::optional<QString> clientStatisticsSettingsUrl;
    std::optional<bool> cloudConnectRelayingEnabled;
    std::optional<bool> cloudConnectRelayingOverSslForced;
    std::optional<bool> cloudConnectUdpHolePunchingEnabled;
    std::optional<std::chrono::seconds> cloudPollingIntervalS;
    std::optional<bool> crossdomainEnabled;
    std::optional<QByteArray> currentStorageEncryptionKey;
    std::optional<QString> defaultVideoCodec;
    std::optional<QString> disabledVendors;
    std::optional<QMap<QString, QList<nx::Uuid>>> downloaderPeers;
    std::optional</*std::chrono::seconds*/ int> ec2AliveUpdateIntervalSec;
    std::optional<bool> enableEdgeRecording;
    std::optional</*std::chrono::days*/ int> eventLogPeriodDays;
    std::optional<bool> exposeDeviceCredentials;
    std::optional<bool> exposeServerEndpoints;
    std::optional<bool> forceAnalyticsDbStoragePermissions;
    std::optional<QString> forceLiveCacheForPrimaryStream;
    std::optional<QString> frameOptionsHeader;
    std::optional<bool> insecureDeprecatedApiEnabled;
    std::optional<bool> insecureDeprecatedApiInUseEnabled;
    std::optional<PersistentUpdateStorage> installedPersistentUpdateStorage;
    std::optional<QString> installedUpdateInformation;
    std::optional<bool> keepIoPortStateIntactOnInitialization;
    std::optional<QString> licenseServer;
    std::optional<QString> lowQualityScreenVideoCodec;
    std::optional<QString> masterCloudSyncList;
    std::optional</*std::chrono::milliseconds*/ int> maxDifferenceBetweenSynchronizedAndInternetTime;
    std::optional<std::chrono::milliseconds> maxDifferenceBetweenSynchronizedAndLocalTimeMs;
    std::optional<int> maxEventLogRecords;
    std::optional<int> maxHttpTranscodingSessions;
    std::optional<qint64> maxP2pAllClientsSizeBytes;
    std::optional<int> maxP2pQueueSizeBytes;
    std::optional<int> maxRecordQueueSizeBytes;
    std::optional<int> maxRecordQueueSizeElements;
    std::optional<int> maxRemoteArchiveSynchronizationThreads;
    std::optional<int> maxRtpRetryCount;
    std::optional</*std::chrono::seconds*/ int> maxRtspConnectDurationSeconds;
    std::optional<int> maxSceneItems;
    std::optional<int> maxVirtualCameraArchiveSynchronizationThreads;
    std::optional<int> mediaBufferSizeForAudioOnlyDeviceKb;
    std::optional<int> mediaBufferSizeKb;
    std::optional<std::chrono::milliseconds> osTimeChangeCheckPeriodMs;
    std::optional<int> proxyConnectTimeoutSec;
    std::optional<ProxyConnectionAccessPolicy> proxyConnectionAccessPolicy;
    std::optional<std::chrono::seconds> remoteSessionTimeoutS;
    std::optional<std::chrono::seconds> remoteSessionUpdateS;
    std::optional<nx::utils::Url> resourceFileUri;
    std::optional<std::chrono::milliseconds> rtpTimeoutMs;
    std::optional<bool> securityForPowerUsers;
    std::optional<bool> sequentialFlirOnvifSearcherEnabled;
    std::optional<QString> serverHeader;
    std::optional<int> sessionsLimit;
    std::optional<int> sessionsLimitPerUser;
    std::optional<bool> showMouseTimelinePreview;
    std::optional<QString> statisticsReportServerApi;
    std::optional<QString> statisticsReportTimeCycle;
    std::optional<QString> statisticsReportUpdateDelay;
    std::optional<QString> supportedOrigins;
    std::optional</*std::chrono::milliseconds*/ int> syncTimeEpsilon;
    std::optional</*std::chrono::milliseconds*/ int> syncTimeExchangePeriod;
    std::optional<PersistentUpdateStorage> targetPersistentUpdateStorage;
    std::optional<QString> targetUpdateInformation;
    std::optional<bool> upnpPortMappingEnabled;
    std::optional<bool> useTextEmailFormat;
    std::optional<bool> useWindowsEmailLineFeed;

    bool operator==(const SaveableSystemSettings&) const = default;
};
#define SaveableSystemSettings_Fields \
    (defaultExportVideoCodec) \
    (systemName)(watermarkSettings)(pixelationSettings)(webSocketEnabled) \
    (autoDiscoveryEnabled)(cameraSettingsOptimization)(statisticsAllowed) \
    (cloudNotificationsLanguage)(auditTrailEnabled)(trafficEncryptionForced) \
    (useHttpsOnlyForCameras)(videoTrafficEncryptionForced)(sessionLimitS)(useSessionLimitForCloud) \
    (storageEncryption)(showServersInTreeForNonAdmins)(updateNotificationsEnabled)(emailSettings) \
    (timeSynchronizationEnabled)(primaryTimeServer)(customReleaseListUrl)(clientUpdateSettings) \
    (backupSettings)(metadataStorageChangePolicy)(allowRegisteringIntegrations) \
    (additionalLocalFsTypes) \
    (arecontRtspEnabled) \
    (auditTrailPeriodDays) \
    (autoDiscoveryResponseEnabled) \
    (autoUpdateThumbnails) \
    (checkVideoStreamPeriodMs) \
    (clientStatisticsSettingsUrl) \
    (cloudConnectRelayingEnabled) \
    (cloudConnectRelayingOverSslForced) \
    (cloudConnectUdpHolePunchingEnabled) \
    (cloudPollingIntervalS) \
    (crossdomainEnabled) \
    (currentStorageEncryptionKey) \
    (defaultVideoCodec) \
    (disabledVendors) \
    (downloaderPeers) \
    (ec2AliveUpdateIntervalSec) \
    (enableEdgeRecording) \
    (eventLogPeriodDays) \
    (exposeDeviceCredentials) \
    (exposeServerEndpoints) \
    (forceAnalyticsDbStoragePermissions) \
    (forceLiveCacheForPrimaryStream) \
    (frameOptionsHeader) \
    (insecureDeprecatedApiEnabled) \
    (insecureDeprecatedApiInUseEnabled) \
    (installedPersistentUpdateStorage) \
    (installedUpdateInformation) \
    (keepIoPortStateIntactOnInitialization) \
    (licenseServer) \
    (lowQualityScreenVideoCodec) \
    (masterCloudSyncList) \
    (maxDifferenceBetweenSynchronizedAndInternetTime) \
    (maxDifferenceBetweenSynchronizedAndLocalTimeMs) \
    (maxEventLogRecords) \
    (maxHttpTranscodingSessions) \
    (maxP2pAllClientsSizeBytes) \
    (maxP2pQueueSizeBytes) \
    (maxRecordQueueSizeBytes) \
    (maxRecordQueueSizeElements) \
    (maxRemoteArchiveSynchronizationThreads) \
    (maxRtpRetryCount) \
    (maxRtspConnectDurationSeconds) \
    (maxSceneItems) \
    (maxVirtualCameraArchiveSynchronizationThreads) \
    (mediaBufferSizeForAudioOnlyDeviceKb) \
    (mediaBufferSizeKb) \
    (osTimeChangeCheckPeriodMs) \
    (proxyConnectTimeoutSec) \
    (proxyConnectionAccessPolicy) \
    (remoteSessionTimeoutS) \
    (remoteSessionUpdateS) \
    (resourceFileUri) \
    (rtpTimeoutMs) \
    (securityForPowerUsers) \
    (sequentialFlirOnvifSearcherEnabled) \
    (serverHeader) \
    (sessionsLimit) \
    (sessionsLimitPerUser) \
    (showMouseTimelinePreview) \
    (statisticsReportServerApi) \
    (statisticsReportTimeCycle) \
    (statisticsReportUpdateDelay) \
    (supportedOrigins) \
    (syncTimeEpsilon) \
    (syncTimeExchangePeriod) \
    (targetPersistentUpdateStorage) \
    (targetUpdateInformation) \
    (upnpPortMappingEnabled) \
    (useSessionLimitForCloud) \
    (useTextEmailFormat) \
    (useWindowsEmailLineFeed)
QN_FUSION_DECLARE_FUNCTIONS(SaveableSystemSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SaveableSystemSettings, SaveableSystemSettings_Fields)
NX_REFLECTION_TAG_TYPE(SaveableSystemSettings, jsonSerializeChronoDurationAsNumber)

struct NX_VMS_API SystemSettings: SaveableSystemSettings
{
    QString cloudAccountName;
    QString cloudHost;
    QString cloudSystemID;
    QString lastMergeMasterId;
    QString lastMergeSlaveId;
    nx::Uuid localSystemId;
    nx::Uuid organizationId;
    std::map<QString, int> specificFeatures;
    int statisticsReportLastNumber = 0;
    QString statisticsReportLastTime;
    QString statisticsReportLastVersion;
    bool system2faEnabled = false;

    bool operator==(const SystemSettings&) const = default;
};
#define SystemSettings_Fields SaveableSystemSettings_Fields \
    (cloudAccountName) \
    (cloudHost) \
    (cloudSystemID) \
    (lastMergeMasterId) \
    (lastMergeSlaveId) \
    (localSystemId) \
    (organizationId) \
    (specificFeatures) \
    (statisticsReportLastNumber) \
    (statisticsReportLastTime) \
    (statisticsReportLastVersion) \
    (system2faEnabled)
QN_FUSION_DECLARE_FUNCTIONS(SystemSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SystemSettings, SystemSettings_Fields)
NX_REFLECTION_TAG_TYPE(SystemSettings, jsonSerializeChronoDurationAsNumber)

} // namespace nx::vms::api
