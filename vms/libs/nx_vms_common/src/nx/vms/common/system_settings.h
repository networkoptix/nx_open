// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <set>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/client_update_settings.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/common/update/persistent_update_storage.h>
#include <utils/email/email_fwd.h>

class QSettings;
class QnAbstractResourcePropertyAdaptor;

namespace nx::vms::api {

struct BackupSettings;
struct EmailSettings;
struct LdapSettings;
struct ResourceParamWithRefData;
struct SystemSettings;
struct WatermarkSettings;
struct PixelationSettings;
enum class ProxyConnectionAccessPolicy;

} // nx::vms::api

namespace nx::vms::common {

struct SystemSettingNames
{
    #define DECLARE_SETTING_NAME(NAME) static const inline QString NAME = NX_FMT( #NAME )

    DECLARE_SETTING_NAME(backupSettings);
    DECLARE_SETTING_NAME(cloudAccountName);
    DECLARE_SETTING_NAME(cloudAuthKey);
    DECLARE_SETTING_NAME(cloudHost);
    DECLARE_SETTING_NAME(cloudNotificationsLanguage);
    DECLARE_SETTING_NAME(cloudPollingIntervalS);
    DECLARE_SETTING_NAME(cloudSystemID); //< TODO: rename it to cloudSystemId
    DECLARE_SETTING_NAME(disabledVendors);
    DECLARE_SETTING_NAME(exposeDeviceCredentials);
    DECLARE_SETTING_NAME(exposeServerEndpoints);
    DECLARE_SETTING_NAME(frameOptionsHeader);
    DECLARE_SETTING_NAME(insecureDeprecatedApiEnabled);
    DECLARE_SETTING_NAME(insecureDeprecatedApiInUseEnabled);
    DECLARE_SETTING_NAME(lastMergeMasterId);
    DECLARE_SETTING_NAME(lastMergeSlaveId);
    DECLARE_SETTING_NAME(ldap);
    DECLARE_SETTING_NAME(licenseServer);
    DECLARE_SETTING_NAME(localSystemId);
    DECLARE_SETTING_NAME(maxHttpTranscodingSessions);
    DECLARE_SETTING_NAME(organizationId);
    DECLARE_SETTING_NAME(primaryTimeServer);
    DECLARE_SETTING_NAME(remoteSessionTimeoutS);
    DECLARE_SETTING_NAME(remoteSessionUpdateS);
    DECLARE_SETTING_NAME(resourceFileUri);
    DECLARE_SETTING_NAME(securityForPowerUsers);
    DECLARE_SETTING_NAME(sessionLimitS);
    DECLARE_SETTING_NAME(useSessionLimitForCloud);
    DECLARE_SETTING_NAME(sessionsLimit);
    DECLARE_SETTING_NAME(sessionsLimitPerUser);
    DECLARE_SETTING_NAME(smtpPassword);
    DECLARE_SETTING_NAME(specificFeatures);
    DECLARE_SETTING_NAME(statisticsReportLastNumber);
    DECLARE_SETTING_NAME(statisticsReportLastTime);
    DECLARE_SETTING_NAME(statisticsReportLastVersion);
    DECLARE_SETTING_NAME(serverHeader);
    DECLARE_SETTING_NAME(supportedOrigins);
    DECLARE_SETTING_NAME(system2faEnabled);
    DECLARE_SETTING_NAME(systemName);
    DECLARE_SETTING_NAME(trafficEncryptionForced);
    DECLARE_SETTING_NAME(videoTrafficEncryptionForced);
    DECLARE_SETTING_NAME(webSocketEnabled);
    DECLARE_SETTING_NAME(pixelationSettings);

    static const inline std::set<QString> kReadOnlyNames = {
        cloudAccountName,
        cloudAuthKey,
        cloudHost,
        cloudSystemID,
        lastMergeMasterId,
        lastMergeSlaveId,
        localSystemId,
        organizationId,
        specificFeatures,
        statisticsReportLastNumber,
        statisticsReportLastTime,
        statisticsReportLastVersion,
        system2faEnabled
    };

    static const inline std::set<QString> kWriteOnlyNames = {
        cloudAuthKey,
    };

    static const inline std::set<QString> kSecurityNames = {
        cloudPollingIntervalS,
        disabledVendors,
        insecureDeprecatedApiEnabled,
        licenseServer,
        maxHttpTranscodingSessions,
        remoteSessionTimeoutS,
        remoteSessionUpdateS,
        resourceFileUri,
        securityForPowerUsers,
        sessionLimitS,
        useSessionLimitForCloud,
        sessionsLimit,
        sessionsLimitPerUser,
        trafficEncryptionForced,
        webSocketEnabled,
        pixelationSettings,
    };

    static const inline std::set<QString> kHiddenNames = {
        ldap,
    };
};

using FileToPeerList = QMap<QString, QList<nx::Uuid>>;

class NX_VMS_COMMON_API SystemSettings:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    typedef QObject base_type;

public:
    using Names = SystemSettingNames;

    SystemSettings(SystemContext* context, QObject* parent = nullptr);
    virtual ~SystemSettings();

    void initialize();

    /** Check if global settings are ready to use. */
    bool isInitialized() const;

    void synchronizeNow();

    /**
     * Save all settings to database
     */
    bool resynchronizeNowSync(nx::utils::MoveOnlyFunc<
        bool(const QString& paramName, const QString& paramValue)> filter = nullptr);

    /**
    * Save modified settings to database
    */
    bool synchronizeNowSync();

    bool takeFromSettings(QSettings* settings, const QnResourcePtr& mediaServer);

    QSet<QString> disabledVendorsSet() const;
    QString disabledVendors() const;
    void setDisabledVendors(QString disabledVendors);

    bool isCameraSettingsOptimizationEnabled() const;
    void setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled);

    /**
     * Allow server auto open streams to cameras sometimes to update camera thumbnail
     */
    bool isAutoUpdateThumbnailsEnabled() const;
    void setAutoUpdateThumbnailsEnabled(bool value);

    /**
     * Override maximum allowed scene items count.
     */
    int maxSceneItemsOverride() const;
    void setMaxSceneItemsOverride(int value);

    /**
     * Send text email instead of HTML email in event rules/actions
     */
    bool isUseTextEmailFormat() const;
    void setUseTextEmailFormat(bool value);

    bool isUseWindowsEmailLineFeed() const;
    void setUseWindowsEmailLineFeed(bool value);

    bool isAuditTrailEnabled() const;
    void setAuditTrailEnabled(bool value);
    std::chrono::days auditTrailPeriodDays() const;
    std::chrono::days eventLogPeriodDays() const;

    bool isTrafficEncryptionForced() const;
    void setTrafficEncryptionForced(bool value);

    bool isVideoTrafficEncryptionForced() const;
    void setVideoTrafficEncryptionForced(bool value);

    bool exposeDeviceCredentials() const;
    void setExposeDeviceCredentials(bool value);

    bool useHttpsOnlyForCameras() const;
    void setUseHttpsOnlyForCameras(bool value);

    bool securityForPowerUsers() const;
    void setSecurityForPowerUsers(bool value = true);

    bool isInsecureDeprecatedApiEnabled() const;
    void enableInsecureDeprecatedApi(bool value = true);

    bool isInsecureDeprecatedApiInUseEnabled() const;
    void enableInsecureDeprecatedApiInUse(bool value = true);

    bool isAutoDiscoveryEnabled() const;
    void setAutoDiscoveryEnabled(bool enabled);

    bool isAutoDiscoveryResponseEnabled() const;
    void setAutoDiscoveryResponseEnabled(bool enabled);

    QnEmailSettings emailSettings() const;
    void setEmailSettings(const QnEmailSettings& emailSettings);

    nx::vms::api::LdapSettings ldap() const;
    void setLdap(nx::vms::api::LdapSettings settings);

    bool isUpdateNotificationsEnabled() const;
    void setUpdateNotificationsEnabled(bool updateNotificationsEnabled);

    nx::vms::api::BackupSettings backupSettings() const;
    void setBackupSettings(const nx::vms::api::BackupSettings& value);

    bool isStatisticsAllowed() const;
    void setStatisticsAllowed(bool value);

    /** Last time when statistics was successfully sent. */
    QDateTime statisticsReportLastTime() const;
    void setStatisticsReportLastTime(const QDateTime& value);

    QString statisticsReportLastVersion() const;
    void setStatisticsReportLastVersion(const QString& value);

    int statisticsReportLastNumber() const;
    void setStatisticsReportLastNumber(int value);

    /** How often should we send statistics in human-readable format like '2d', '30m', etc. */
    QString statisticsReportTimeCycle() const;
    void setStatisticsReportTimeCycle(const QString& value);

    QString statisticsReportUpdateDelay() const;
    void setStatisticsReportUpdateDelay(const QString& value);

    bool isUpnpPortMappingEnabled() const;
    void setUpnpPortMappingEnabled(bool value);

    /** local systemId. Media servers connect if this value equal */
    nx::Uuid localSystemId() const;
    void setLocalSystemId(const nx::Uuid& value);

    /**
     *
     * Last merge operation id. It set same value for master and slave hosts during merge
     */
    nx::Uuid lastMergeMasterId() const;
    void setLastMergeMasterId(const nx::Uuid& value);

    nx::Uuid lastMergeSlaveId() const;
    void setLastMergeSlaveId(const nx::Uuid& value);

    nx::utils::Url clientStatisticsSettingsUrl() const;

    QString statisticsReportServerApi() const;
    void setStatisticsReportServerApi(const QString& value);

    int maxEventLogRecords() const;
    void setMaxEventLogRecords(int value);

    std::chrono::seconds aliveUpdateInterval() const;
    void setAliveUpdateInterval(std::chrono::seconds newInterval) const;

    // -- Time synchronization parameters.

    bool isTimeSynchronizationEnabled() const;
    void setTimeSynchronizationEnabled(bool value);

    nx::Uuid primaryTimeServer() const;
    void setPrimaryTimeServer(const nx::Uuid& value);

    std::chrono::milliseconds maxDifferenceBetweenSynchronizedAndInternetTime() const;
    std::chrono::milliseconds maxDifferenceBetweenSynchronizedAndLocalTime() const;

    std::chrono::milliseconds osTimeChangeCheckPeriod() const;
    void setOsTimeChangeCheckPeriod(std::chrono::milliseconds value);

    std::chrono::milliseconds syncTimeExchangePeriod() const;
    void setSyncTimeExchangePeriod(std::chrono::milliseconds value);

    std::chrono::milliseconds syncTimeEpsilon() const;
    void setSyncTimeEpsilon(std::chrono::milliseconds value);

    // -- (end) Time synchronization parameters.

    bool takeCameraOwnershipWithoutLock() const;

    // -- Cloud settings

    QString cloudAccountName() const;
    void setCloudAccountName(const QString& value);

    nx::Uuid organizationId() const;
    void setOrganizationId(const nx::Uuid& value);

    QString cloudSystemId() const;
    void setCloudSystemId(const QString& value);

    QString cloudAuthKey() const;
    void setCloudAuthKey(const QString& value);

    QString systemName() const;
    void setSystemName(const QString& value);

    void resetCloudParams();

    /**
     * Should be used instead of `resetCloudParams()` for automatically triggered actions
     * so User actions override them.
     */
    void resetCloudParamsWithLowPriority();

    // -- Misc settings

    bool isNewSystem() const { return localSystemId().isNull(); }
    /** Media server put cloud host here from nx::network::AppInfo::defaultCloudHost */
    QString cloudHost() const;
    void setCloudHost(const QString& value);

    bool crossdomainEnabled() const;
    void setCrossdomainEnabled(bool value);

    bool arecontRtspEnabled() const;
    void setArecontRtspEnabled(bool newVal) const;

    int maxRtpRetryCount() const;
    void setMaxRtpRetryCount(int newVal);

    bool sequentialFlirOnvifSearcherEnabled() const;
    void setSequentialFlirOnvifSearcherEnabled(bool newVal);

    int rtpFrameTimeoutMs() const;
    void setRtpFrameTimeoutMs(int newValue);

    std::chrono::seconds proxyConnectTimeout() const;

    nx::vms::api::ProxyConnectionAccessPolicy proxyConnectionAccessPolicy() const;
    void setProxyConnectionAccessPolicy(nx::vms::api::ProxyConnectionAccessPolicy value);

    bool cloudConnectUdpHolePunchingEnabled() const;
    bool cloudConnectRelayingEnabled() const;
    bool cloudConnectRelayingOverSslForced() const;

    std::chrono::seconds maxRtspConnectDuration() const;
    void setMaxRtspConnectDuration(std::chrono::seconds newValue);

    /*!
        \a QnAbstractResourcePropertyAdaptor class methods are thread-safe
        \note returned list is not changed during \a SystemSettings instance life-time
    */
    const QList<QnAbstractResourcePropertyAdaptor*>& allSettings() const;

    // Returns only settings with default value.
    QList<const QnAbstractResourcePropertyAdaptor*> allDefaultSettings() const;

    static bool isGlobalSetting(const nx::vms::api::ResourceParamWithRefData& param);

    int maxP2pQueueSizeBytes() const;
    qint64 maxP2pQueueSizeForAllClientsBytes() const;

    int maxRecorderQueueSizeBytes() const;
    int maxRecorderQueueSizePackets() const;

    int maxHttpTranscodingSessions() const;

    bool isWebSocketEnabled() const;
    void setWebSocketEnabled(bool value);

    bool isEdgeRecordingEnabled() const;
    void setEdgeRecordingEnabled(bool enabled);

    int maxRemoteArchiveSynchronizationThreads() const;
    void setMaxRemoteArchiveSynchronizationThreads(int newValue);

    nx::utils::Url customReleaseListUrl() const;
    void setCustomReleaseListUrl(const nx::utils::Url& url);

    QByteArray targetUpdateInformation() const;
    void setTargetUpdateInformation(const QByteArray& updateInformation);

    nx::vms::common::update::PersistentUpdateStorage targetPersistentUpdateStorage() const;
    void setTargetPersistentUpdateStorage(
        const nx::vms::common::update::PersistentUpdateStorage& data);

    nx::vms::common::update::PersistentUpdateStorage installedPersistentUpdateStorage() const;
    void setInstalledPersistentUpdateStorage(const nx::vms::common::update::PersistentUpdateStorage& data);

    QByteArray installedUpdateInformation() const;
    void setInstalledUpdateInformation(const QByteArray& updateInformation);

    FileToPeerList downloaderPeers() const;
    void setdDownloaderPeers(const FileToPeerList& downloaderPeers);

    nx::vms::api::ClientUpdateSettings clientUpdateSettings() const;
    void setClientUpdateSettings(const nx::vms::api::ClientUpdateSettings& settings);

    int maxVirtualCameraArchiveSynchronizationThreads() const;
    void setMaxVirtualCameraArchiveSynchronizationThreads(int newValue);

    nx::vms::api::WatermarkSettings watermarkSettings() const;
    void setWatermarkSettings(const nx::vms::api::WatermarkSettings& settings) const;

    nx::vms::api::PixelationSettings pixelationSettings() const;
    void setPixelationSettings(const nx::vms::api::PixelationSettings& settings);

    std::optional<std::chrono::seconds> sessionTimeoutLimit() const;
    void setSessionTimeoutLimit(std::optional<std::chrono::seconds> value);

    bool useSessionTimeoutLimitForCloud() const;
    void setUseSessionTimeoutLimitForCloud(bool value);

    int sessionsLimit() const;
    void setSessionsLimit(int value);

    int sessionsLimitPerUser() const;
    void setSessionsLimitPerUser(int value);

    std::chrono::seconds remoteSessionUpdate() const;
    void setRemoteSessionUpdate(std::chrono::seconds value);

    std::chrono::seconds remoteSessionTimeout() const;
    void setRemoteSessionTimeout(std::chrono::seconds value);

    QString defaultVideoCodec() const;
    void setDefaultVideoCodec(const QString& value);

    QString defaultExportVideoCodec() const;
    void setDefaultExportVideoCodec(const QString& value);

    QString lowQualityScreenVideoCodec() const;
    void setLowQualityScreenVideoCodec(const QString& value);

    QString forceLiveCacheForPrimaryStream() const;
    void setForceLiveCacheForPrimaryStream(const QString& value);

    nx::vms::api::MetadataStorageChangePolicy metadataStorageChangePolicy() const;
    void setMetadataStorageChangePolicy(nx::vms::api::MetadataStorageChangePolicy value);

    std::map<QString, int> specificFeatures() const;
    void setSpecificFeatures(std::map<QString, int> value);

    QString licenseServerUrl() const;
    void setLicenseServerUrl(const QString& value);

    nx::utils::Url resourceFileUri() const;

    QString cloudNotificationsLanguage() const;
    void setCloudNotificationsLanguage(const QString& value);

    QString additionalLocalFsTypes() const;

    bool keepIoPortStateIntactOnInitialization() const;
    void setKeepIoPortStateIntactOnInitialization(bool value);

    int mediaBufferSizeKb() const;
    void setMediaBufferSizeKb(int value);

    int mediaBufferSizeForAudioOnlyDeviceKb() const;
    void setMediaBufferSizeForAudioOnlyDeviceKb(int value);

    bool forceAnalyticsDbStoragePermissions() const;

    std::chrono::milliseconds checkVideoStreamPeriod() const;
    void setCheckVideoStreamPeriod(std::chrono::milliseconds value);

    bool useStorageEncryption() const;
    void setUseStorageEncryption(bool value);

    QByteArray currentStorageEncryptionKey() const;
    void setCurrentStorageEncryptionKey(const QByteArray& value);

    bool showServersInTreeForNonAdmins() const;
    void setShowServersInTreeForNonAdmins(bool value);

    QString serverHeader() const;
    void setServerHeader(const QString& value);
    std::string makeServerHeader() const;

    QString supportedOrigins() const;
    void setSupportedOrigins(const QString& value);

    QString frameOptionsHeader() const;
    void setFrameOptionsHeader(const QString& value);

    bool exposeServerEndpoints() const;
    void setExposeServerEndpoints(bool value);

    bool showMouseTimelinePreview() const;
    void setShowMouseTimelinePreview(bool value);

    bool system2faEnabled() const;
    void setSystem2faEnabled(bool value);

    std::vector<nx::Uuid> masterCloudSyncList() const;
    void setMasterCloudSyncList(const std::vector<nx::Uuid>& idList) const;

    std::chrono::seconds cloudPollingInterval() const;
    void setCloudPollingInterval(std::chrono::seconds value);

    void update(const vms::api::SystemSettings& value);

    bool isAllowRegisteringIntegrationsEnabled() const;
    void setAllowRegisteringIntegrationsEnabled(bool value);

signals:
    void initialized();

    void systemNameChanged();
    void localSystemIdChanged();
    void disabledVendorsChanged();
    void auditTrailEnableChanged();
    void auditTrailPeriodDaysChanged();
    void eventLogPeriodDaysChanged();
    void cameraSettingsOptimizationChanged();
    void useHttpsOnlyForCamerasChanged();
    void autoUpdateThumbnailsChanged();
    void maxSceneItemsChanged();
    void useTextEmailFormatChanged();
    void useWindowsEmailLineFeedChanged();
    void autoDiscoveryChanged();
    void emailSettingsChanged();
    void ldapSettingsChanged();
    void statisticsAllowedChanged();
    void trafficEncryptionForcedChanged();
    void videoTrafficEncryptionForcedChanged();
    void updateNotificationsChanged();
    void upnpPortMappingEnabledChanged();
    void ec2ConnectionSettingsChanged(const QString& key);
    void cloudSettingsChanged();
    void organizationIdChanged();
    void cloudCredentialsChanged();
    void timeSynchronizationSettingsChanged();
    void cloudConnectUdpHolePunchingEnabledChanged();
    void cloudConnectRelayingEnabledChanged();
    void cloudConnectRelayingOverSslForcedChanged();
    void customReleaseListUrlChanged();
    void targetUpdateInformationChanged();
    void installedUpdateInformationChanged();
    void downloaderPeersChanged();
    void clientUpdateSettingsChanged();
    void targetPersistentUpdateStorageChanged();
    void installedPersistentUpdateStorageChanged();
    void watermarkChanged();
    void pixelationSettingsChanged();
    void sessionTimeoutChanged();
    void sessionsLimitChanged();
    void sessionsLimitPerUserChanged();
    void remoteSessionUpdateChanged();
    void useStorageEncryptionChanged();
    void currentStorageEncryptionKeyChanged();
    void cloudNotificationsLanguageChanged();
    void showServersInTreeForNonAdminsChanged();
    void backupSettingsChanged();
    void showMouseTimelinePreviewChanged();
    void cloudStorageUpdatePeriodChanged();
    void masterCloudSyncChanged();
    void securityForPowerUsersChanged();
    void cloudPollingIntervalChanged();
    void allowRegisteringIntegrationsChanged();

private:
    typedef QList<QnAbstractResourcePropertyAdaptor*> AdaptorList;

    AdaptorList initEmailAdaptors();
    AdaptorList initStaticticsAdaptors();
    AdaptorList initConnectionAdaptors();
    AdaptorList initTimeSynchronizationAdaptors();
    AdaptorList initCloudAdaptors();
    AdaptorList initMiscAdaptors();

    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
