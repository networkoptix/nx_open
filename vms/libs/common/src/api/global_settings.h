#pragma once

#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <common/common_globals.h>
#include <common/common_module_aware.h>
#include <core/resource/resource_fwd.h>
#include <nx_ec/data/api_fwd.h>
#include <utils/common/connective.h>
#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>
#include <utils/common/optional.h>

#include <nx/utils/singleton.h>

class QnAbstractResourcePropertyAdaptor;

template<class T>
class QnResourcePropertyAdaptor;
class QSettings;

struct QnWatermarkSettings;

namespace nx::settings_names {

const QString kNameDisabledVendors(lit("disabledVendors"));
const QString kNameCameraSettingsOptimization(lit("cameraSettingsOptimization"));
const QString kNameAutoUpdateThumbnails(lit("autoUpdateThumbnails"));
const QString kMaxSceneItemsOverrideKey(lit("maxSceneItems"));
const QString kUseTextEmailFormat(lit("useTextEmailFormat"));
const QString kUseWindowsEmailLineFeed(lit("useWindowsEmailLineFeed"));
const QString kNameAuditTrailEnabled(lit("auditTrailEnabled"));
const QString kAuditTrailPeriodDaysName(lit("auditTrailPeriodDays"));
const QString kNameTrafficEncryptionForced(lit("trafficEncryptionForced"));
const QString kNameVideoTrafficEncryptionForced(lit("videoTrafficEncryptionForced"));
const QString kEventLogPeriodDaysName(lit("eventLogPeriodDays"));
const QString kNameHost(lit("smtpHost"));
const QString kNamePort(lit("smtpPort"));
const QString kNameUser(lit("smtpUser"));
const QString kNamePassword(lit("smtpPassword"));
const QString kNameConnectionType(lit("smtpConnectionType"));
const QString kNameSimple(lit("smtpSimple"));
const QString kNameTimeout(lit("smtpTimeout"));
const QString kNameFrom(lit("emailFrom"));
const QString kNameSignature(lit("emailSignature"));
const QString kNameSupportEmail(lit("emailSupportEmail"));
const QString kNameUpdateNotificationsEnabled(lit("updateNotificationsEnabled"));

const QString kNameTimeSynchronizationEnabled(lit("timeSynchronizationEnabled"));
const QString kNamePrimaryTimeServer(lit("primaryTimeServer"));

/* Max rtt for internet time synchronization request */
const QString kMaxDifferenceBetweenSynchronizedAndInternetTime(
    lit("maxDifferenceBetweenSynchronizedAndInternetTime"));

/* Max rtt for server to server or client to server time synchronization request */
const QString kMaxDifferenceBetweenSynchronizedAndLocalTime(
    lit("maxDifferenceBetweenSynchronizedAndLocalTimeMs"));

/* Period to check local time for changes */
const QString kOsTimeChangeCheckPeriod(lit("osTimeChangeCheckPeriodMs"));

/* Period to synchronize time via network */
const QString kSyncTimeExchangePeriod(lit("syncTimeExchangePeriod"));

const QString kNameAutoDiscoveryEnabled(lit("autoDiscoveryEnabled"));
const QString kNameAutoDiscoveryResponseEnabled(lit("autoDiscoveryResponseEnabled"));
const QString kNameBackupQualities(lit("backupQualities"));
const QString kNameBackupNewCamerasByDefault(lit("backupNewCamerasByDefault"));
const QString kNameCrossdomainEnabled(lit("crossdomainEnabled"));
const QString kCloudHostName(lit("cloudHost"));

const QString kNameStatisticsAllowed(lit("statisticsAllowed"));
const QString kNameStatisticsReportLastTime(lit("statisticsReportLastTime"));
const QString kNameStatisticsReportLastVersion(lit("statisticsReportLastVersion"));
const QString kNameStatisticsReportLastNumber(lit("statisticsReportLastNumber"));
const QString kNameStatisticsReportTimeCycle(lit("statisticsReportTimeCycle"));
const QString kNameStatisticsReportUpdateDelay(lit("statisticsReportUpdateDelay"));
const QString kNameLocalSystemId(lit("localSystemId"));
const QString kNameSystemName(lit("systemName"));
const QString kNameStatisticsReportServerApi(lit("statisticsReportServerApi"));
const QString kNameSettingsUrlParam(lit("clientStatisticsSettingsUrl"));

const QString ldapUri(lit("ldapUri"));
const QString ldapAdminDn(lit("ldapAdminDn"));
const QString ldapAdminPassword(lit("ldapAdminPassword"));
const QString ldapSearchBase(lit("ldapSearchBase"));
const QString ldapSearchFilter(lit("ldapSearchFilter"));
const QString ldapSearchTimeoutS(lit("ldapSearchTimeoutS"));
const int ldapSearchTimeoutSDefault(30);

const QString kNameCloudAccountName(lit("cloudAccountName"));
const QString kNameCloudSystemId(lit("cloudSystemID")); //< todo: rename it to cloudSystemId
const QString kNameCloudAuthKey(lit("cloudAuthKey"));
const QString kNameUpnpPortMappingEnabled(lit("upnpPortMappingEnabled"));
const QString kConnectionKeepAliveTimeoutKey(lit("ec2ConnectionKeepAliveTimeoutSec"));
const QString kKeepAliveProbeCountKey(lit("ec2KeepAliveProbeCount"));

static const QString kTargetUpdateInformationName = lit("targetUpdateInformation");
static const QString kInstalledUpdateInformationName = lit("installedUpdateInformation");
static const QString kDownloaderPeersName = lit("downloaderPeers");

const QString kWatermarkSettingsName(lit("watermarkSettings"));
static const QString kSessionLimit("sessionLimitMinutes");
const QString kDefaultVideoCodec(lit("defaultVideoCodec"));
const QString kDefaultExportVideoCodec(lit("defaultExportVideoCodec"));
const QString kLowQualityScreenVideoCodec(lit("lowQualityScreenVideoCodec"));
const QString kForceLiveCacheForPrimaryStream(lit("forceLiveCacheForPrimaryStream"));

} // namespace nx::settings_names

using FileToPeerList = QMap<QString, QList<QnUuid>>;

class QnGlobalSettings: public Connective<QObject>, public /*mixin*/ QnCommonModuleAware
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
    bool resynchronizeNowSync();

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
    int auditTrailPeriodDays() const;
    int eventLogPeriodDays() const;

    bool isTrafficEncriptionForced() const;
    void setTrafficEncriptionForced(bool value);

    bool isVideoTrafficEncriptionForced() const;
    void setVideoTrafficEncryptionForced(bool value);

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

    Qn::CameraBackupQualities backupQualities() const;
    void setBackupQualities(Qn::CameraBackupQualities value);

    bool backupNewCamerasByDefault() const;
    void setBackupNewCamerasByDefault(bool value);

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

    QString clientStatisticsSettingsUrl() const;

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

    std::chrono::seconds maxRtspConnectDuration() const;
    void setMaxRtspConnectDuration(std::chrono::seconds newValue);

    /*!
        \a QnAbstractResourcePropertyAdaptor class methods are thread-safe
        \note returned list is not changed during \a QnGlobalSettings instance life-time
    */
    const QList<QnAbstractResourcePropertyAdaptor*>& allSettings() const;

    static bool isGlobalSetting(const nx::vms::api::ResourceParamWithRefData& param);

    int maxRecorderQueueSizeBytes() const;
    int maxRecorderQueueSizePackets() const;

    int maxWebMTranscoders() const;

    bool isEdgeRecordingEnabled() const;
    void setEdgeRecordingEnabled(bool enabled);

    int maxRemoteArchiveSynchronizationThreads() const;
    void setMaxRemoteArchiveSynchronizationThreads(int newValue);

    QByteArray targetUpdateInformation() const;
    void setTargetUpdateInformation(const QByteArray& updateInformation);

    QByteArray installedUpdateInformation() const;
    void setInstalledUpdateInformation(const QByteArray& updateInformation);

    FileToPeerList downloaderPeers() const;
    void setdDownloaderPeers(const FileToPeerList& downloaderPeers);

    int maxWearableArchiveSynchronizationThreads() const;
    void setMaxWearableArchiveSynchronizationThreads(int newValue);

    QnWatermarkSettings watermarkSettings() const;
    void setWatermarkSettings(const QnWatermarkSettings& settings) const;

    std::chrono::minutes sessionTimeoutLimit() const;
    void setSessionTimeoutLimit(std::chrono::minutes value);

    QString defaultVideoCodec() const;
    void setDefaultVideoCodec(const QString& value);

    QString defaultExportVideoCodec() const;
    void setDefaultExportVideoCodec(const QString& value);

    QString lowQualityScreenVideoCodec() const;
    void setLowQualityScreenVideoCodec(const QString& value);

    QString forceLiveCacheForPrimaryStream() const;
    void setForceLiveCacheForPrimaryStream(const QString& value);

signals:
    void initialized();

    void systemNameChanged();
    void localSystemIdChanged();
    void localSystemIdChangedDirect();
    void disabledVendorsChanged();
    void auditTrailEnableChanged();
    void auditTrailPeriodDaysChanged();
    void eventLogPeriodDaysChanged();
    void cameraSettingsOptimizationChanged();
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
    void targetUpdateInformationChanged();
    void installedUpdateInformationChanged();
    void downloaderPeersChanged();
    void watermarkChanged();
    void sessionTimeoutChanged();

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
    QnResourcePropertyAdaptor<bool>* m_trafficEncryptionForcedAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_videoTrafficEncryptionForcedAdaptor = nullptr;

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
    QnResourcePropertyAdaptor<Qn::CameraBackupQualities>* m_backupQualitiesAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_backupNewCamerasByDefaultAdaptor = nullptr;

    // set of statistics settings adaptors
    QnResourcePropertyAdaptor<QnOptionalBool>* m_statisticsAllowedAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportLastTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportLastVersionAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_statisticsReportLastNumberAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportTimeCycleAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportUpdateDelayAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_upnpPortMappingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_localSystemIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_statisticsReportServerApiAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_clientStatisticsSettingsUrlAdaptor = nullptr;

    // set of email settings adaptors
    QnResourcePropertyAdaptor<QString>* m_serverAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_fromAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_userAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_passwordAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_signatureAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_supportLinkAdaptor = nullptr;
    QnResourcePropertyAdaptor<QnEmail::ConnectionType>* m_connectionTypeAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_portAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_timeoutAdaptor = nullptr;
    /** Flag that we are using simple smtp settings set */
    QnResourcePropertyAdaptor<bool>* m_simpleAdaptor = nullptr;

    // set of ldap settings adaptors
    QnResourcePropertyAdaptor<QUrl>* m_ldapUriAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapAdminDnAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapAdminPasswordAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapSearchBaseAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_ldapSearchFilterAdaptor = nullptr;
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

    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizeBytes = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizePackets = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxWebMTranscoders = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxRtpRetryCount = nullptr;

    QnResourcePropertyAdaptor<int>* m_rtpFrameTimeoutMs = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxRtspConnectDuration = nullptr;

    QnResourcePropertyAdaptor<bool>* m_cloudConnectUdpHolePunchingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_cloudConnectRelayingEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<bool>* m_edgeRecordingEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxRemoteArchiveSynchronizationThreads = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxWearableArchiveSynchronizationThreads = nullptr;

    QnResourcePropertyAdaptor<QByteArray>* m_targetUpdateInformationAdaptor = nullptr;
    QnResourcePropertyAdaptor<QByteArray>* m_installedUpdateInformationAdaptor = nullptr;
    QnResourcePropertyAdaptor<FileToPeerList>* m_downloaderPeersAdaptor = nullptr;
    QnResourcePropertyAdaptor<QnWatermarkSettings>* m_watermarkSettingsAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_sessionTimeoutLimitMinutesAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_defaultVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_defaultExportVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_lowQualityScreenVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_forceLiveCacheForPrimaryStreamAdaptor = nullptr;

    AdaptorList m_allAdaptors;

    mutable QnMutex m_mutex;
    QnUserResourcePtr m_admin;
};

#define qnGlobalSettings commonModule()->globalSettings()
