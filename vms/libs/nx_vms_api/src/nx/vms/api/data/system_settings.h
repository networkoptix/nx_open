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
#include "user_session_settings.h"
#include "watermark_settings.h"

namespace nx::vms::api {

struct SaveableSettingsBase
{
    std::optional<QString> defaultExportVideoCodec;
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
    std::optional<int> auditTrailPeriodDays; //< TODO: Make std::chrono.
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
    std::optional<int> ec2AliveUpdateIntervalSec; //< TODO: Make std::chrono.
    std::optional<bool> enableEdgeRecording;
    std::optional<int> eventLogPeriodDays; //< TODO: Make std::chrono.
    std::optional<bool> exposeDeviceCredentials;
    std::optional<bool> exposeServerEndpoints;
    std::optional<bool> forceAnalyticsDbStoragePermissions;
    std::optional<QString> forceLiveCacheForPrimaryStream;
    std::optional<QString> frameOptionsHeader;
    std::optional<bool> insecureDeprecatedApiEnabled;
    std::optional<bool> insecureDeprecatedApiInUseEnabled;
    std::optional<PersistentUpdateStorage> installedPersistentUpdateStorage;
    std::optional<QByteArray> installedUpdateInformation;
    std::optional<bool> keepIoPortStateIntactOnInitialization;
    std::optional<QString> licenseServer;
    std::optional<QString> lowQualityScreenVideoCodec;
    std::optional<QString> masterCloudSyncList;
    std::optional<int> maxDifferenceBetweenSynchronizedAndInternetTime; //< TODO: Make std::chrono.
    std::optional<std::chrono::milliseconds> maxDifferenceBetweenSynchronizedAndLocalTimeMs;
    std::optional<int> maxEventLogRecords;
    std::optional<int> maxHttpTranscodingSessions;
    std::optional<qint64> maxP2pAllClientsSizeBytes;
    std::optional<int> maxP2pQueueSizeBytes;
    std::optional<int> maxRecordQueueSizeBytes;
    std::optional<int> maxRecordQueueSizeElements;
    std::optional<int> maxRemoteArchiveSynchronizationThreads;
    std::optional<int> maxRtpRetryCount;
    std::optional<int> maxRtspConnectDurationSeconds; //< TODO: Make std::chrono.
    std::optional<int> maxSceneItems;
    std::optional<int> maxVirtualCameraArchiveSynchronizationThreads;
    std::optional<int> mediaBufferSizeForAudioOnlyDeviceKb;
    std::optional<int> mediaBufferSizeKb;
    std::optional<std::chrono::milliseconds> osTimeChangeCheckPeriodMs;
    std::optional<int> proxyConnectTimeoutSec;
    std::optional<ProxyConnectionAccessPolicy> proxyConnectionAccessPolicy;
    std::optional<nx::utils::Url> resourceFileUri;
    std::optional<std::chrono::milliseconds> rtpTimeoutMs;
    std::optional<bool> securityForPowerUsers;
    std::optional<bool> sequentialFlirOnvifSearcherEnabled;
    std::optional<QString> serverHeader;
    std::optional<bool> showMouseTimelinePreview;
    std::optional<QString> statisticsReportServerApi;
    std::optional<QString> statisticsReportTimeCycle;
    std::optional<QString> statisticsReportUpdateDelay;
    std::optional<QString> supportedOrigins;
    std::optional<int> syncTimeEpsilon; //< TODO: Make std::chrono.
    std::optional<int> syncTimeExchangePeriod; //< TODO: Make std::chrono.
    std::optional<PersistentUpdateStorage> targetPersistentUpdateStorage;
    std::optional<QByteArray> targetUpdateInformation;
    std::optional<bool> upnpPortMappingEnabled;
    std::optional<bool> useTextEmailFormat;
    std::optional<bool> useWindowsEmailLineFeed;

    bool operator==(const SaveableSettingsBase&) const = default;
};
#define SaveableSettingsBase_Fields \
    (defaultExportVideoCodec) \
    (watermarkSettings)(pixelationSettings)(webSocketEnabled) \
    (autoDiscoveryEnabled)(cameraSettingsOptimization)(statisticsAllowed) \
    (cloudNotificationsLanguage)(auditTrailEnabled)(trafficEncryptionForced) \
    (useHttpsOnlyForCameras)(videoTrafficEncryptionForced) \
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
    (resourceFileUri) \
    (rtpTimeoutMs) \
    (securityForPowerUsers) \
    (sequentialFlirOnvifSearcherEnabled) \
    (serverHeader) \
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
    (useTextEmailFormat) \
    (useWindowsEmailLineFeed)

struct NX_VMS_API SettingsBase
{
    QString cloudAccountName;
    QString cloudHost;
    QString lastMergeMasterId;
    QString lastMergeSlaveId;
    nx::Uuid organizationId;
    std::map<QString, int> specificFeatures;
    int statisticsReportLastNumber = 0;
    QString statisticsReportLastTime;
    QString statisticsReportLastVersion;

    bool operator==(const SettingsBase&) const = default;
};
#define SettingsBase_Fields \
    (cloudAccountName) \
    (cloudHost) \
    (lastMergeMasterId) \
    (lastMergeSlaveId) \
    (organizationId) \
    (specificFeatures) \
    (statisticsReportLastNumber) \
    (statisticsReportLastTime) \
    (statisticsReportLastVersion)

#define SaveableSiteSettings_Fields SaveableSettingsBase_Fields \
    (userSessionSettings)(siteName)
#define UnsaveableSiteSettings_Fields SettingsBase_Fields \
    (cloudId)(localId)(enabled2fa)
#define SiteSettings_Fields SaveableSiteSettings_Fields UnsaveableSiteSettings_Fields
enum class SiteSettingName
{
    none,
    #ifndef Q_MOC_RUN
        #define VALUE(R, DATA, ITEM) ITEM,
        BOOST_PP_SEQ_FOR_EACH(VALUE, _, SiteSettings_Fields)
        #undef VALUE
    #endif // Q_MOC_RUN
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(SiteSettingName*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<SiteSettingName>;
    const auto& itemName = nx::reflect::enumeration::detail::itemName;
    return visitor(Item{SiteSettingName::none, ""}
        #ifndef Q_MOC_RUN
            #define VALUE(R, ENUM, ITEM) , Item{ENUM::ITEM, itemName(BOOST_PP_STRINGIZE(ITEM))}
            BOOST_PP_SEQ_FOR_EACH(VALUE, SiteSettingName, SiteSettings_Fields)
            #undef VALUE
        #endif // Q_MOC_RUN
    );
}

struct SiteSettingsFilter
{
    // TODO: Add support for multiple names.
    SiteSettingName name = SiteSettingName::none;

    SiteSettingsFilter() = default;
    bool operator==(const SiteSettingsFilter&) const = default;
    SiteSettingsFilter(SiteSettingName id): name(id) {}

    SiteSettingName getId() const { return name; }
    void setId(SiteSettingName id) { name = id; };
};
#define SiteSettingsFilter_Fields (name)
QN_FUSION_DECLARE_FUNCTIONS(SiteSettingsFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SiteSettingsFilter, SiteSettingsFilter_Fields)

/**%apidoc
 * %param[unused] name
 */
struct NX_VMS_API SaveableSiteSettings: SaveableSettingsBase, SiteSettingsFilter
{
    std::optional<UserSessionSettings> userSessionSettings;
    std::optional<QString> siteName;

    using SiteSettingsFilter::getId;
    using SiteSettingsFilter::setId;
    bool operator==(const SaveableSiteSettings&) const = default;
};
QN_FUSION_DECLARE_FUNCTIONS(SaveableSiteSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SaveableSiteSettings, SaveableSiteSettings_Fields)
NX_REFLECTION_TAG_TYPE(SaveableSiteSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SaveableSiteSettings, jsonSerializeInt64AsString)

/**%apidoc
 * %param[unused] emailSettings.password
 */
struct NX_VMS_API SiteSettings: SaveableSiteSettings, SettingsBase
{
    QString cloudId;
    nx::Uuid localId;
    bool enabled2fa = false;

    bool operator==(const SiteSettings&) const = default;
    size_t size() const { return 1u; }
    bool empty() const { return false; }
    QJsonValue front() const;
};
QN_FUSION_DECLARE_FUNCTIONS(SiteSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SiteSettings, SiteSettings_Fields)
NX_REFLECTION_TAG_TYPE(SiteSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SiteSettings, jsonSerializeInt64AsString)

struct NX_VMS_API SaveableSystemSettings: SaveableSettingsBase, UserSessionSettings
{
    std::optional<QString> systemName;

    SaveableSystemSettings() = default;
    bool operator==(const SaveableSystemSettings&) const = default;

    SaveableSystemSettings(
        SaveableSettingsBase saveableSettingsBase,
        UserSessionSettings userSessionSettings = {},
        std::optional<QString> systemName = {})
        :
        SaveableSettingsBase(std::move(saveableSettingsBase)),
        UserSessionSettings(std::move(userSessionSettings)),
        systemName(std::move(systemName))
    {
    }

    SaveableSystemSettings(SaveableSiteSettings settings):
        SaveableSettingsBase(std::move(settings)),
        UserSessionSettings(
            std::move(settings.userSessionSettings).value_or(UserSessionSettings{})),
        systemName(std::move(settings.siteName))
    {
    }

    operator SaveableSiteSettings() const
    {
        SaveableSiteSettings result;
        static_cast<SaveableSettingsBase&>(result) = *this;
        result.userSessionSettings = *this;
        result.siteName = systemName;
        return result;
    }
};
#define SaveableSystemSettings_Fields \
    SaveableSettingsBase_Fields \
    UserSessionSettings_Fields \
    (systemName)
QN_FUSION_DECLARE_FUNCTIONS(SaveableSystemSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SaveableSystemSettings, SaveableSystemSettings_Fields)
NX_REFLECTION_TAG_TYPE(SaveableSystemSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SaveableSystemSettings, jsonSerializeInt64AsString)

struct NX_VMS_API SystemSettings: SaveableSystemSettings, SettingsBase
{
    QString cloudSystemID;
    nx::Uuid localSystemId;
    bool system2faEnabled = false;

    SystemSettings() = default;
    bool operator==(const SystemSettings&) const = default;

    SystemSettings(SiteSettings settings):
        SaveableSystemSettings(std::move(settings)),
        SettingsBase(std::move(settings)),
        cloudSystemID(std::move(settings.cloudId)),
        localSystemId(std::move(settings.localId)),
        system2faEnabled(settings.enabled2fa)
    {
    }

    operator SiteSettings() const
    {
        SiteSettings result;
        static_cast<SaveableSiteSettings&>(result) = *this;
        static_cast<SettingsBase&>(result) = *this;
        result.cloudId = cloudSystemID;
        result.localId = localSystemId;
        result.enabled2fa = system2faEnabled;
        return result;
    }
};
#define UnsaveableSystemSettings_Fields SettingsBase_Fields \
    (cloudSystemID)(localSystemId)(system2faEnabled)
#define SystemSettings_Fields SaveableSystemSettings_Fields UnsaveableSystemSettings_Fields
QN_FUSION_DECLARE_FUNCTIONS(SystemSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SystemSettings, SystemSettings_Fields)
NX_REFLECTION_TAG_TYPE(SystemSettings, jsonSerializeChronoDurationAsNumber)
NX_REFLECTION_TAG_TYPE(SystemSettings, jsonSerializeInt64AsString)

enum class SystemSettingName
{
    #ifndef Q_MOC_RUN
        #define VALUE(R, DATA, ITEM) ITEM,
        BOOST_PP_SEQ_FOR_EACH(VALUE, _, SystemSettings_Fields)
        #undef VALUE
    #endif // Q_MOC_RUN
    sessionLimitMinutes,
    none,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(SystemSettingName*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<SystemSettingName>;
    const auto& itemName = nx::reflect::enumeration::detail::itemName;
    return visitor(
        Item{SystemSettingName::sessionLimitMinutes, "sessionLimitMinutes"},
        Item{SystemSettingName::none, ""}
        #ifndef Q_MOC_RUN
            #define VALUE(R, ENUM, ITEM) , Item{ENUM::ITEM, itemName(BOOST_PP_STRINGIZE(ITEM))}
            BOOST_PP_SEQ_FOR_EACH(VALUE, SystemSettingName, SystemSettings_Fields)
            #undef VALUE
        #endif // Q_MOC_RUN
    );
}

struct SystemSettingsFilter
{
    SystemSettingName name = SystemSettingName::none;

    SystemSettingsFilter() = default;
    SystemSettingsFilter(QString id)
    {
        nx::reflect::enumeration::fromString(id.toStdString(), &name);
    }

    const SystemSettingsFilter& getId() const { return *this; }
    void setId(SystemSettingsFilter filter) { this->name = filter.name; };
};
#define SystemSettingsFilter_Fields (name)
QN_FUSION_DECLARE_FUNCTIONS(SystemSettingsFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(SystemSettingsFilter, SystemSettingsFilter_Fields)

} // namespace nx::vms::api
