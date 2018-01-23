#pragma once

#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/singleton.h>
#include <utils/common/connective.h>
#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>
#include <utils/common/optional.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <nx_ec/data/api_resource_data.h>
#include <common/common_module_aware.h>

class QnAbstractResourcePropertyAdaptor;

template<class T>
class QnResourcePropertyAdaptor;
class QSettings;

namespace nx {
namespace settings_names {

const QString kNameDisabledVendors(lit("disabledVendors"));
const QString kNameCameraSettingsOptimization(lit("cameraSettingsOptimization"));
const QString kNameAutoUpdateThumbnails(lit("autoUpdateThumbnails"));
const QString kMaxSceneItemsOverrideKey(lit("maxSceneItems"));
const QString kUseTextEmailFormat(lit("useTextEmailFormat"));
const QString kNameAuditTrailEnabled(lit("auditTrailEnabled"));
const QString kAuditTrailPeriodDaysName(lit("auditTrailPeriodDays"));
const QString kEventLogPeriodDaysName(lit("eventLogPeriodDays"));
const QString kNameHost(lit("smtpHost"));
const QString kNamePort(lit("smtpPort"));
const QString kNameUser(lit("smtpUser"));
const QString kNamePassword(lit("smptPassword"));
const QString kNameConnectionType(lit("smtpConnectionType"));
const QString kNameSimple(lit("smtpSimple"));
const QString kNameTimeout(lit("smtpTimeout"));
const QString kNameFrom(lit("emailFrom"));
const QString kNameSignature(lit("emailSignature"));
const QString kNameSupportEmail(lit("emailSupportEmail"));
const QString kNameUpdateNotificationsEnabled(lit("updateNotificationsEnabled"));
const QString kNameTimeSynchronizationEnabled(lit("timeSynchronizationEnabled"));
const QString kNameSynchronizeTimeWithInternet(lit("synchronizeTimeWithInternet"));
const QString kMaxDifferenceBetweenSynchronizedAndInternetTime(
    lit("maxDifferenceBetweenSynchronizedAndInternetTime"));
const QString kMaxDifferenceBetweenSynchronizedAndLocalTime(
    lit("maxDifferenceBetweenSynchronizedAndLocalTimeMs"));
const QString kNameAutoDiscoveryEnabled(lit("autoDiscoveryEnabled"));
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

const QString kNameCloudAccountName(lit("cloudAccountName"));
const QString kNameCloudSystemId(lit("cloudSystemID")); //< todo: rename it to cloudSystemId
const QString kNameCloudAuthKey(lit("cloudAuthKey"));
const QString kNameUpnpPortMappingEnabled(lit("upnpPortMappingEnabled"));
const QString kConnectionKeepAliveTimeoutKey(lit("ec2ConnectionKeepAliveTimeoutSec"));
const QString kKeepAliveProbeCountKey(lit("ec2KeepAliveProbeCount"));

} // namespace settings_names
} // namespace nx

class QnGlobalSettings: public Connective<QObject>, public QnCommonModuleAware
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

    bool isAuditTrailEnabled() const;
    void setAuditTrailEnabled(bool value);
    int auditTrailPeriodDays() const;
    int eventLogPeriodDays() const;

    bool isAutoDiscoveryEnabled() const;
    void setAutoDiscoveryEnabled(bool enabled);

    QnEmailSettings emailSettings() const;
    void setEmailSettings(const QnEmailSettings &settings);

    QnLdapSettings ldapSettings() const;
    void setLdapSettings(const QnLdapSettings &settings);

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
    void setStatisticsReportServerApi(const QString &value);

    std::chrono::seconds connectionKeepAliveTimeout() const;
    void setConnectionKeepAliveTimeout(std::chrono::seconds newTimeout);

    int keepAliveProbeCount() const;
    void setKeepAliveProbeCount(int newProbeCount);

    std::chrono::seconds aliveUpdateInterval() const;
    void setAliveUpdateInterval(std::chrono::seconds newInterval) const;

    std::chrono::seconds serverDiscoveryPingTimeout() const;
    void setServerDiscoveryPingTimeout(std::chrono::seconds newInterval) const;

    std::chrono::seconds serverDiscoveryAliveCheckTimeout() const;

    bool isTimeSynchronizationEnabled() const;
    void setTimeSynchronizationEnabled(bool value);

    bool isSynchronizingTimeWithInternet() const;
    void setSynchronizingTimeWithInternet(bool value);

    std::chrono::milliseconds maxDifferenceBetweenSynchronizedAndInternetTime() const;
    std::chrono::milliseconds maxDifferenceBetweenSynchronizedAndLocalTime() const;
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

    static bool isGlobalSetting(const ec2::ApiResourceParamWithRefData& param);

    int maxRecorderQueueSizeBytes() const;
    int maxRecorderQueueSizePackets() const;

    bool hanwhaDeleteProfilesOnInitIfNeeded() const;
    void setHanwhaDeleteProfilesOnInitIfNeeded(bool deleteProfiles);

    bool isEdgeRecordingEnabled() const;
    void setEdgeRecordingEnabled(bool enabled);

    int maxRemoteArchiveSynchronizationThreads() const;
    void setMaxRemoteArchiveSynchronizationThreads(int newValue);

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
    void autoDiscoveryChanged();
    void emailSettingsChanged();
    void ldapSettingsChanged();
    void statisticsAllowedChanged();
    void updateNotificationsChanged();
    void upnpPortMappingEnabledChanged();
    void ec2ConnectionSettingsChanged(const QString& key);
    void cloudSettingsChanged();
    void cloudCredentialsChanged();
    void timeSynchronizationSettingsChanged();
    void cloudConnectUdpHolePunchingEnabledChanged();
    void cloudConnectRelayingEnabledChanged();

private:
    typedef QList<QnAbstractResourcePropertyAdaptor*> AdaptorList;

    AdaptorList initEmailAdaptors();
    AdaptorList initLdapAdaptors();
    AdaptorList initStaticticsAdaptors();
    AdaptorList initConnectionAdaptors();
    AdaptorList initTimeSynchronizationAdaptors();
    AdaptorList initCloudAdaptors();
    AdaptorList initMiscAdaptors();

    void at_adminUserAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QnResourcePropertyAdaptor<bool> *m_cameraSettingsOptimizationAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_autoUpdateThumbnailsAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxSceneItemsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_useTextEmailFormatAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_auditTrailEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_auditTrailPeriodDaysAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* m_eventLogPeriodDaysAdaptor = nullptr;

    QnResourcePropertyAdaptor<QString> *m_disabledVendorsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_autoDiscoveryEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_updateNotificationsEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_timeSynchronizationEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_synchronizeTimeWithInternetAdaptor = nullptr;
    QnResourcePropertyAdaptor<int> *m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<int> *m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<Qn::CameraBackupQualities> *m_backupQualitiesAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_backupNewCamerasByDefaultAdaptor = nullptr;

    // set of statistics settings adaptors
    QnResourcePropertyAdaptor<QnOptionalBool> *m_statisticsAllowedAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportLastTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportLastVersionAdaptor = nullptr;
    QnResourcePropertyAdaptor<int> *m_statisticsReportLastNumberAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportTimeCycleAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportUpdateDelayAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool> *m_upnpPortMappingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_localSystemIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportServerApiAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_clientStatisticsSettingsUrlAdaptor = nullptr;

    // set of email settings adaptors
    QnResourcePropertyAdaptor<QString> *m_serverAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_fromAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_userAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_passwordAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_signatureAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_supportLinkAdaptor = nullptr;
    QnResourcePropertyAdaptor<QnEmail::ConnectionType> *m_connectionTypeAdaptor = nullptr;
    QnResourcePropertyAdaptor<int> *m_portAdaptor = nullptr;
    QnResourcePropertyAdaptor<int> *m_timeoutAdaptor = nullptr;
    /** Flag that we are using simple smtp settings set */
    QnResourcePropertyAdaptor<bool> *m_simpleAdaptor = nullptr;

    // set of ldap settings adaptors
    QnResourcePropertyAdaptor<QUrl> *m_ldapUriAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_ldapAdminDnAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_ldapAdminPasswordAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_ldapSearchBaseAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString> *m_ldapSearchFilterAdaptor = nullptr;

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
    QnResourcePropertyAdaptor<QString>* m_systemNameAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_arecontRtspEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_sequentialFlirOnvifSearcherEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* m_cloudHostAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizeBytes = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizePackets = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxRtpRetryCount = nullptr;

    QnResourcePropertyAdaptor<int>* m_rtpFrameTimeoutMs = nullptr;
    QnResourcePropertyAdaptor<int>* m_maxRtspConnectDuration = nullptr;

    QnResourcePropertyAdaptor<bool>* m_cloudConnectUdpHolePunchingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* m_cloudConnectRelayingEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<bool>* m_hanwhaDeleteProfilesOnInitIfNeeded = nullptr;

    QnResourcePropertyAdaptor<bool>* m_edgeRecordingEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* m_maxRemoteArchiveSynchronizationThreads = nullptr;

    AdaptorList m_allAdaptors;

    mutable QnMutex m_mutex;
    QnUserResourcePtr m_admin;
};

#define qnGlobalSettings commonModule()->globalSettings()
