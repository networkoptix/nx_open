// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <set>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <common/common_globals.h>
#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/vms/common/api/client_update_settings.h>
#include <nx/vms/common/update/persistent_update_storage.h>
#include <utils/common/connective.h>
#include <utils/common/ldap_fwd.h>
#include <utils/common/optional.h>
#include <utils/email/email_fwd.h>

class QnAbstractResourcePropertyAdaptor;

template<class T>
class QnResourcePropertyAdaptor;
class QSettings;

struct QnWatermarkSettings;

namespace nx::vms::api {
    struct ResourceParamWithRefData;
    struct BackupSettings;
} // namespace nx::vms::api

namespace nx::settings_names {

NX_VMS_API extern const QString kNameDisabledVendors;
NX_VMS_API extern const QString kNameCameraSettingsOptimization;
NX_VMS_API extern const QString kNameAutoUpdateThumbnails;
NX_VMS_API extern const QString kMaxSceneItemsOverrideKey;
NX_VMS_API extern const QString kUseTextEmailFormat;
NX_VMS_API extern const QString kUseWindowsEmailLineFeed;
NX_VMS_API extern const QString kNameAuditTrailEnabled;
NX_VMS_API extern const QString kAuditTrailPeriodDaysName;
NX_VMS_API extern const QString kNameTrafficEncryptionForced;
NX_VMS_API extern const QString kNameVideoTrafficEncryptionForced;
NX_VMS_API extern const QString kNameInsecureDeprecatedApiEnabled;
NX_VMS_API extern const QString kEventLogPeriodDaysName;
NX_VMS_API extern const QString kNameHost;
NX_VMS_API extern const QString kNamePort;
NX_VMS_API extern const QString kNameUser;
NX_VMS_API extern const QString kNameSmtpPassword;
NX_VMS_API extern const QString kNameConnectionType;
NX_VMS_API extern const QString kNameSimple;
NX_VMS_API extern const QString kNameTimeout;
NX_VMS_API extern const QString kNameFrom;
NX_VMS_API extern const QString kNameSignature;
NX_VMS_API extern const QString kNameSupportEmail;
NX_VMS_API extern const QString kNameUpdateNotificationsEnabled;

NX_VMS_API extern const QString kNameTimeSynchronizationEnabled;
NX_VMS_API extern const QString kNamePrimaryTimeServer;

/* Max rtt for internet time synchronization request */
NX_VMS_API extern const QString kMaxDifferenceBetweenSynchronizedAndInternetTime;

/* Max rtt for server to server or client to server time synchronization request */
NX_VMS_API extern const QString kMaxDifferenceBetweenSynchronizedAndLocalTime;

/* Period to check local time for changes */
NX_VMS_API extern const QString kOsTimeChangeCheckPeriod;

/* Period to synchronize time via network */
NX_VMS_API extern const QString kSyncTimeExchangePeriod;
NX_VMS_API extern const QString kSyncTimeEpsilon;

NX_VMS_API extern const QString kNameAutoDiscoveryEnabled;
NX_VMS_API extern const QString kNameAutoDiscoveryResponseEnabled;
NX_VMS_API extern const QString kNameBackupSettings;
NX_VMS_API extern const QString kNameCrossdomainEnabled;
NX_VMS_API extern const QString kCloudHostName;

NX_VMS_API extern const QString kNameStatisticsAllowed;
NX_VMS_API extern const QString kNameStatisticsReportLastTime;
NX_VMS_API extern const QString kNameStatisticsReportLastVersion;
NX_VMS_API extern const QString kNameStatisticsReportLastNumber;
NX_VMS_API extern const QString kNameStatisticsReportTimeCycle;
NX_VMS_API extern const QString kNameStatisticsReportUpdateDelay;
NX_VMS_API extern const QString kNameLocalSystemId;
NX_VMS_API extern const QString kNameLastMergeMasterId;
NX_VMS_API extern const QString kNameLastMergeSlaveId;
NX_VMS_API extern const QString kNameSystemName;
NX_VMS_API extern const QString kNameStatisticsReportServerApi;
NX_VMS_API extern const QString kNameSettingsUrlParam;
NX_VMS_API extern const QString kNameSpecificFeatures;

NX_VMS_API extern const QString ldapUri;
NX_VMS_API extern const QString ldapAdminDn;
NX_VMS_API extern const QString ldapAdminPassword;
NX_VMS_API extern const QString ldapSearchBase;
NX_VMS_API extern const QString ldapSearchFilter;
NX_VMS_API extern const QString ldapPasswordExpirationPeriodMs;
NX_VMS_API extern const QString ldapSearchTimeoutS;
constexpr const int ldapSearchTimeoutSDefault(30);

NX_VMS_API extern const QString kNameCloudAccountName;
NX_VMS_API extern const QString kNameCloudSystemId;
NX_VMS_API extern const QString kNameCloudAuthKey;
NX_VMS_API extern const QString kNameUpnpPortMappingEnabled;
NX_VMS_API extern const QString kConnectionKeepAliveTimeoutKey;
NX_VMS_API extern const QString kKeepAliveProbeCountKey;

NX_VMS_API extern const QString kTargetUpdateInformationName;
NX_VMS_API extern const QString kInstalledUpdateInformationName;
NX_VMS_API extern const QString kTargetPersistentUpdateStorageName;
NX_VMS_API extern const QString kInstalledPersistentUpdateStorageName;
NX_VMS_API extern const QString kDownloaderPeersName;
NX_VMS_API extern const QString kClientUpdateSettings;

NX_VMS_API extern const QString kWatermarkSettingsName;
NX_VMS_API extern const QString kDefaultVideoCodec;
NX_VMS_API extern const QString kDefaultExportVideoCodec;
NX_VMS_API extern const QString kLowQualityScreenVideoCodec;
NX_VMS_API extern const QString kForceLiveCacheForPrimaryStream;
NX_VMS_API extern const QString kMetadataStorageChangePolicyName;

NX_VMS_API extern const QString kShowServersInTreeForNonAdmins;
NX_VMS_API extern const QString kShowMouseTimelinePreview;

NX_VMS_API extern const std::set<QString> kReadOnlyNames;

NX_VMS_API extern const std::set<QString> kWriteOnlyNames;

} // namespace nx::settings_names

using FileToPeerList = QMap<QString, QList<QnUuid>>;

class NX_VMS_COMMON_API QnGlobalSettings:
    public Connective<QObject>,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnGlobalSettings(QObject* parent = nullptr);
    virtual ~QnGlobalSettings();

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
    bool isTrafficEncryptionForcedExplicitlyDefined() const;
    void setTrafficEncryptionForced(bool value);

    bool isVideoTrafficEncryptionForced() const;
    void setVideoTrafficEncryptionForced(bool value);

    bool exposeDeviceCredentials() const;
    void setExposeDeviceCredentials(bool value);

    bool useHttpsOnlyCameras() const;
    void setUseHttpsOnlyCameras(bool value);

    bool isInsecureDeprecatedApiEnabled() const;
    void setInsecureDeprecatedApiEnabled(bool value);

    bool isAutoDiscoveryEnabled() const;
    void setAutoDiscoveryEnabled(bool enabled);

    bool isAutoDiscoveryResponseEnabled() const;
    void setAutoDiscoveryResponseEnabled(bool enabled);

    QnEmailSettings emailSettings() const;
    void setEmailSettings(const QnEmailSettings& settings);

    QnLdapSettings ldapSettings() const;
    void setLdapSettings(const QnLdapSettings& settings);

    bool isUpdateNotificationsEnabled() const;
    void setUpdateNotificationsEnabled(bool updateNotificationsEnabled);

    nx::vms::api::BackupSettings backupSettings() const;
    void setBackupSettings(const nx::vms::api::BackupSettings& value);

    bool isStatisticsAllowedDefined() const;
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
    QnUuid localSystemId() const;
    void setLocalSystemId(const QnUuid& value);

    /**
     *
     * Last merge operation id. It set same value for master and slave hosts during merge
     */
    QnUuid lastMergeMasterId() const;
    void setLastMergeMasterId(const QnUuid& value);

    QnUuid lastMergeSlaveId() const;
    void setLastMergeSlaveId(const QnUuid& value);

    nx::utils::Url clientStatisticsSettingsUrl() const;

    QString statisticsReportServerApi() const;
    void setStatisticsReportServerApi(const QString& value);

    std::chrono::seconds connectionKeepAliveTimeout() const;
    void setConnectionKeepAliveTimeout(std::chrono::seconds newTimeout);

    int keepAliveProbeCount() const;
    void setKeepAliveProbeCount(int newProbeCount);

    int maxEventLogRecords() const;
    void setMaxEventLogRecords(int value);

    std::chrono::seconds aliveUpdateInterval() const;
    void setAliveUpdateInterval(std::chrono::seconds newInterval) const;

    std::chrono::seconds serverDiscoveryPingTimeout() const;
    void setServerDiscoveryPingTimeout(std::chrono::seconds newInterval) const;

    std::chrono::seconds serverDiscoveryAliveCheckTimeout() const;

    // -- Time synchronization parameters.

    bool isTimeSynchronizationEnabled() const;
    void setTimeSynchronizationEnabled(bool value);

    QnUuid primaryTimeServer() const;
    void setPrimaryTimeServer(const QnUuid& value);

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

    QString cloudSystemId() const;
    void setCloudSystemId(const QString& value);

    QString cloudAuthKey() const;
    void setCloudAuthKey(const QString& value);

    QString systemName() const;
    void setSystemName(const QString& value);

    void resetCloudParams();

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

    bool cloudConnectUdpHolePunchingEnabled() const;
    bool cloudConnectRelayingEnabled() const;
    bool cloudConnectRelayingOverSslForced() const;

    std::chrono::seconds maxRtspConnectDuration() const;
    void setMaxRtspConnectDuration(std::chrono::seconds newValue);

    /*!
        \a QnAbstractResourcePropertyAdaptor class methods are thread-safe
        \note returned list is not changed during \a QnGlobalSettings instance life-time
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

    nx::vms::common::api::ClientUpdateSettings clientUpdateSettings() const;
    void setClientUpdateSettings(const nx::vms::common::api::ClientUpdateSettings& settings);

    int maxVirtualCameraArchiveSynchronizationThreads() const;
    void setMaxVirtualCameraArchiveSynchronizationThreads(int newValue);

    QnWatermarkSettings watermarkSettings() const;
    void setWatermarkSettings(const QnWatermarkSettings& settings) const;

    std::optional<std::chrono::minutes> sessionTimeoutLimit() const;
    void setSessionTimeoutLimit(std::optional<std::chrono::minutes> value);

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

    QString pushNotificationsLanguage() const;
    void setPushNotificationsLanguage(const QString& value);

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

    QString supportedOrigins() const;
    void setSupportedOrigins(const QString& value);

    QString frameOptionsHeader() const;
    void setFrameOptionsHeader(const QString& value);

    bool showMouseTimelinePreview() const;
    void setShowMouseTimelinePreview(bool value);

signals:
    void initialized();

    void systemNameChanged();
    void localSystemIdChanged();
    void disabledVendorsChanged();
    void auditTrailEnableChanged();
    void auditTrailPeriodDaysChanged();
    void eventLogPeriodDaysChanged();
    void cameraSettingsOptimizationChanged();
    void useHttpsOnlyCamerasChanged();
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
    void cloudCredentialsChanged();
    void timeSynchronizationSettingsChanged();
    void cloudConnectUdpHolePunchingEnabledChanged();
    void cloudConnectRelayingEnabledChanged();
    void cloudConnectRelayingOverSslForcedChanged();
    void targetUpdateInformationChanged();
    void installedUpdateInformationChanged();
    void downloaderPeersChanged();
    void clientUpdateSettingsChanged();
    void targetPersistentUpdateStorageChanged();
    void installedPersistentUpdateStorageChanged();
    void watermarkChanged();
    void sessionTimeoutChanged();
    void sessionsLimitChanged();
    void sessionsLimitPerUserChanged();
    void remoteSessionUpdateChanged();
    void useStorageEncryptionChanged();
    void currentStorageEncryptionKeyChanged();
    void pushNotificationsLanguageChanged();
    void showServersInTreeForNonAdminsChanged();
    void backupSettingsChanged();
    void showMouseTimelinePreviewChanged();

private:
    typedef QList<QnAbstractResourcePropertyAdaptor*> AdaptorList;

    AdaptorList initEmailAdaptors();
    AdaptorList initLdapAdaptors();
    AdaptorList initStaticticsAdaptors();
    AdaptorList initConnectionAdaptors();
    AdaptorList initTimeSynchronizationAdaptors();
    AdaptorList initCloudAdaptors();
    AdaptorList initMiscAdaptors();

    void at_adminUserAdded(const QnResourcePtr& resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);

private:
    QnResourcePropertyAdaptor<bool>* m_cameraSettingsOptimizationAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_autoUpdateThumbnailsAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxSceneItemsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_useTextEmailFormatAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_useWindowsEmailLineFeedAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_auditTrailEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_auditTrailPeriodDaysAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_eventLogPeriodDaysAdaptor = nullptr;
    QnResourcePropertyAdaptor<QnOptionalBool>* m_trafficEncryptionForcedAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_videoTrafficEncryptionForcedAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_exposeDeviceCredentialsAdaptor = nullptr;

    QnResourcePropertyAdaptor<QString>* m_disabledVendorsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_autoDiscoveryEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_autoDiscoveryResponseEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_updateNotificationsEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_timeSynchronizationEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_synchronizeTimeWithInternetAdaptor = nullptr;
    QnResourcePropertyAdaptor<QnUuid> *m_primaryTimeServerAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_osTimeChangeCheckPeriodAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_syncTimeExchangePeriodAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_syncTimeEpsilonAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::api::BackupSettings>* m_backupSettingsAdaptor = nullptr;

    // set of statistics settings adaptors
    QnResourcePropertyAdaptor<QnOptionalBool>* m_statisticsAllowedAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportLastTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportLastVersionAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_statisticsReportLastNumberAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportTimeCycleAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportUpdateDelayAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_upnpPortMappingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_localSystemIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_lastMergeMasterIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_lastMergeSlaveIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportServerApiAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_clientStatisticsSettingsUrlAdaptor = nullptr;

    // set of email settings adaptors
    QnResourcePropertyAdaptor<QString>* m_serverAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_fromAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_userAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_smtpPasswordAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_signatureAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_supportLinkAdaptor = nullptr;
    QnResourcePropertyAdaptor<QnEmail::ConnectionType>* m_connectionTypeAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_portAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_timeoutAdaptor = nullptr;
    /** Flag that we are using simple smtp settings set */
    QnResourcePropertyAdaptor<bool>* m_simpleAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_smtpNameAdaptor = nullptr;

    // set of ldap settings adaptors
    QnResourcePropertyAdaptor<QUrl>* m_ldapUriAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapAdminDnAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapAdminPasswordAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapSearchBaseAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapSearchFilterAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_ldapPasswordExpirationPeriodAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_ldapSearchTimeoutSAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_ec2ConnectionKeepAliveTimeoutAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_ec2KeepAliveProbeCountAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_ec2AliveUpdateIntervalAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_serverDiscoveryPingTimeoutAdaptor = nullptr;
    /** seconds */
    QnResourcePropertyAdaptor<int>* m_proxyConnectTimeoutAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_takeCameraOwnershipWithoutLock = nullptr;

    // set of cloud adaptors
    QnResourcePropertyAdaptor<QString>* m_cloudAccountNameAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_cloudSystemIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_cloudAuthKeyAdaptor = nullptr;

    // misc adaptors
    QnResourcePropertyAdaptor<int>* m_maxEventLogRecordsAdaptor = nullptr;

    QnResourcePropertyAdaptor<QString>* m_systemNameAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_arecontRtspEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_sequentialFlirOnvifSearcherEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_cloudHostAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_crossdomainEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxP2pQueueSizeBytes = nullptr;
    QnResourcePropertyAdaptor<qint64>* m_maxP2pQueueSizeForAllClientsBytes = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizeBytes = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizePackets = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxHttpTranscodingSessions = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxRtpRetryCount = nullptr;

    QnResourcePropertyAdaptor<int>* m_rtpFrameTimeoutMs = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxRtspConnectDuration = nullptr;

    QnResourcePropertyAdaptor<bool>* m_cloudConnectUdpHolePunchingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_cloudConnectRelayingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_cloudConnectRelayingOverSslForcedAdaptor = nullptr;

    QnResourcePropertyAdaptor<bool>* m_edgeRecordingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_webSocketEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxRemoteArchiveSynchronizationThreads = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxVirtualCameraArchiveSynchronizationThreads = nullptr;

    QnResourcePropertyAdaptor<QByteArray>* m_targetUpdateInformationAdaptor = nullptr;
    QnResourcePropertyAdaptor<QByteArray>* m_installedUpdateInformationAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::common::update::PersistentUpdateStorage>* m_targetPersistentUpdateStorageAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::common::update::PersistentUpdateStorage>* m_installedPersistentUpdateStorageAdaptor = nullptr;
    QnResourcePropertyAdaptor<FileToPeerList>* m_downloaderPeersAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::common::api::ClientUpdateSettings>*
        m_clientUpdateSettingsAdaptor = nullptr;
    QnResourcePropertyAdaptor<QnWatermarkSettings>* m_watermarkSettingsAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_sessionTimeoutLimitMinutesAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_sessionsLimitAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_sessionsLimitPerUserAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_remoteSessionUpdateAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_remoteSessionTimeoutAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_defaultVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_defaultExportVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_lowQualityScreenVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_forceLiveCacheForPrimaryStreamAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::api::MetadataStorageChangePolicy>* m_metadataStorageChangePolicyAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::map<QString, int>>* m_specificFeaturesAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_licenseServerUrlAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::utils::Url>* m_resourceFileUriAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_pushNotificationsLanguageAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_additionalLocalFsTypesAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_keepIoPortStateIntactOnInitializationAdaptor= nullptr;
    QnResourcePropertyAdaptor<int>* m_mediaBufferSizeKbAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_mediaBufferSizeKbForAudioOnlyDeviceAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_forceAnalyticsDbStoragePermissionsAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_checkVideoStreamPeriodMsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_useStorageEncryptionAdaptor = nullptr;
    QnResourcePropertyAdaptor<QByteArray>* m_currentStorageEncryptionKeyAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_showServersInTreeForNonAdminsAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_supportedOriginsAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_frameOptionsHeaderAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_useHttpsOnlyCamerasAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_insecureDeprecatedApiEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_showMouseTimelinePreviewAdaptor = nullptr;

    AdaptorList m_allAdaptors;

    mutable nx::Mutex m_mutex;
    QnUserResourcePtr m_admin;
};

#define qnGlobalSettings commonModule()->globalSettings()
