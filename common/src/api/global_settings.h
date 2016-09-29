#pragma once

#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <QtCore/QObject>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/singleton.h>
#include <utils/common/connective.h>
#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>
#include <utils/common/optional.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <nx_ec/data/api_resource_data.h>

class QnAbstractResourcePropertyAdaptor;

template<class T>
class QnResourcePropertyAdaptor;
class QSettings;

class QnGlobalSettings: public Connective<QObject>, public Singleton<QnGlobalSettings> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnGlobalSettings(QObject *parent = NULL);
    virtual ~QnGlobalSettings();

    /** Check if global settings are ready to use. */
    bool isInitialized() const;

    void synchronizeNow();
    bool synchronizeNowSync();
    bool takeFromSettings(QSettings* settings, const QnResourcePtr& mediaServer);

    QSet<QString> disabledVendorsSet() const;
    QString disabledVendors() const;
    void setDisabledVendors(QString disabledVendors);

    bool isCameraSettingsOptimizationEnabled() const;
    void setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled);

    bool isAuditTrailEnabled() const;
    void setAuditTrailEnabled(bool value);

    bool isServerAutoDiscoveryEnabled() const;
    void setServerAutoDiscoveryEnabled(bool enabled);

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

    static const QString kNameUpnpPortMappingEnabled;
    bool isUpnpPortMappingEnabled() const;
    void setUpnpPortMappingEnabled(bool value);

    static const QString kNameLocalSystemID;
    QnUuid localSystemID() const;
    void setLocalSystemID(const QnUuid &value);

    QString clientStatisticsSettingsUrl() const;

    QString statisticsReportServerApi() const;
    void setStatisticsReportServerApi(const QString &value);

    static const QString kConnectionKeepAliveTimeoutKey;
    std::chrono::seconds connectionKeepAliveTimeout() const;
    void setConnectionKeepAliveTimeout(std::chrono::seconds newTimeout);

    static const QString kKeepAliveProbeCountKey;
    int keepAliveProbeCount() const;
    void setKeepAliveProbeCount(int newProbeCount);

    std::chrono::seconds aliveUpdateInterval() const;
    void setAliveUpdateInterval(std::chrono::seconds newInterval) const;

    std::chrono::seconds serverDiscoveryPingTimeout() const;
    void setServerDiscoveryPingTimeout(std::chrono::seconds newInterval) const;

    std::chrono::seconds serverDiscoveryAliveCheckTimeout() const;
    bool isTimeSynchronizationEnabled() const;
    bool takeCameraOwnershipWithoutLock() const;

    // -- Cloud settings

    static const QString kNameCloudAccountName;
    QString cloudAccountName() const;
    void setCloudAccountName(const QString& value);

    static const QString kNameCloudSystemID;
    QString cloudSystemID() const;
    void setCloudSystemID(const QString& value);

    static const QString kNameCloudAuthKey;
    QString cloudAuthKey() const;
    void setCloudAuthKey(const QString& value);

    static const QString kNameSystemName;
    QString systemName() const;
    void setSystemName(const QString& value);

    void resetCloudParams();

    // -- Misc settings

    /** System is not set, it has default admin password and not linked to the cloud. */
    bool isNewSystem() const;
    void setNewSystem(bool value);

    /** Media server put cloud host here from QnAppInfo::defaultCloudHost */
    QString cloudHost() const;
    void setCloudHost(const QString& value);

    bool arecontRtspEnabled() const;
    void setArecontRtspEnabled(bool newVal) const;

    std::chrono::seconds proxyConnectTimeout() const;

    /*!
        \a QnAbstractResourcePropertyAdaptor class methods are thread-safe
        \note returned list is not changed during \a QnGlobalSettings instance life-time
    */
    const QList<QnAbstractResourcePropertyAdaptor*>& allSettings() const;

    bool isGlobalSetting(const ec2::ApiResourceParamWithRefData& param) const;

    int maxRecorderQueueSizeBytes() const;
    int maxRecorderQueueSizePackets() const;

signals:
    void initialized();

    void systemNameChanged();
    void localSystemIdChanged();
    void disabledVendorsChanged();
    void auditTrailEnableChanged();
    void cameraSettingsOptimizationChanged();
    void serverAutoDiscoveryChanged();
    void emailSettingsChanged();
    void ldapSettingsChanged();
    void statisticsAllowedChanged();
    void updateNotificationsChanged();
    void upnpPortMappingEnabledChanged();
    void ec2ConnectionSettingsChanged(const QString& key);
    void cloudSettingsChanged();
    void newSystemChanged();

private:
    typedef QList<QnAbstractResourcePropertyAdaptor*> AdaptorList;

    AdaptorList initEmailAdaptors();
    AdaptorList initLdapAdaptors();
    AdaptorList initStaticticsAdaptors();
    AdaptorList initConnectionAdaptors();
    AdaptorList initCloudAdaptors();
    AdaptorList initMiscAdaptors();

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QnResourcePropertyAdaptor<bool> *m_cameraSettingsOptimizationAdaptor;
    QnResourcePropertyAdaptor<bool> *m_auditTrailEnabledAdaptor;
    QnResourcePropertyAdaptor<QString> *m_disabledVendorsAdaptor;
    QnResourcePropertyAdaptor<bool> *m_serverAutoDiscoveryEnabledAdaptor;
    QnResourcePropertyAdaptor<bool> *m_updateNotificationsEnabledAdaptor;
    QnResourcePropertyAdaptor<bool> *m_timeSynchronizationEnabledAdaptor;
    QnResourcePropertyAdaptor<Qn::CameraBackupQualities> *m_backupQualitiesAdaptor;
    QnResourcePropertyAdaptor<bool> *m_backupNewCamerasByDefaultAdaptor;

    // set of statistics settings adaptors
    QnResourcePropertyAdaptor<QnOptionalBool> *m_statisticsAllowedAdaptor;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportLastTimeAdaptor;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportLastVersionAdaptor;
    QnResourcePropertyAdaptor<int> *m_statisticsReportLastNumberAdaptor;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportTimeCycleAdaptor;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportUpdateDelayAdaptor;
    QnResourcePropertyAdaptor<bool> *m_upnpPortMappingEnabledAdaptor;
    QnResourcePropertyAdaptor<QnUuid> *m_localSystemIdAdaptor;
    QnResourcePropertyAdaptor<QString> *m_statisticsReportServerApiAdaptor;
    QnResourcePropertyAdaptor<QString> *m_clientStatisticsSettingsUrlAdaptor;

    // set of email settings adaptors
    QnResourcePropertyAdaptor<QString> *m_serverAdaptor;
    QnResourcePropertyAdaptor<QString> *m_fromAdaptor;
    QnResourcePropertyAdaptor<QString> *m_userAdaptor;
    QnResourcePropertyAdaptor<QString> *m_passwordAdaptor;
    QnResourcePropertyAdaptor<QString> *m_signatureAdaptor;
    QnResourcePropertyAdaptor<QString> *m_supportLinkAdaptor;
    QnResourcePropertyAdaptor<QnEmail::ConnectionType> *m_connectionTypeAdaptor;
    QnResourcePropertyAdaptor<int> *m_portAdaptor;
    QnResourcePropertyAdaptor<int> *m_timeoutAdaptor;
    /** Flag that we are using simple smtp settings set */
    QnResourcePropertyAdaptor<bool> *m_simpleAdaptor;   //TODO: #GDM #Common think where else we can store it

    // set of ldap settings adaptors
    QnResourcePropertyAdaptor<QUrl> *m_ldapUriAdaptor;
    QnResourcePropertyAdaptor<QString> *m_ldapAdminDnAdaptor;
    QnResourcePropertyAdaptor<QString> *m_ldapAdminPasswordAdaptor;
    QnResourcePropertyAdaptor<QString> *m_ldapSearchBaseAdaptor;
    QnResourcePropertyAdaptor<QString> *m_ldapSearchFilterAdaptor;

    QnResourcePropertyAdaptor<int>* m_ec2ConnectionKeepAliveTimeoutAdaptor;
    QnResourcePropertyAdaptor<int>* m_ec2KeepAliveProbeCountAdaptor;
    QnResourcePropertyAdaptor<int>* m_ec2AliveUpdateIntervalAdaptor;
    QnResourcePropertyAdaptor<int>* m_serverDiscoveryPingTimeoutAdaptor;
    /** seconds */
    QnResourcePropertyAdaptor<int>* m_proxyConnectTimeoutAdaptor;
    QnResourcePropertyAdaptor<bool>* m_takeCameraOwnershipWithoutLock;

    // set of cloud adaptors
    QnResourcePropertyAdaptor<QString>* m_cloudAccountNameAdaptor;
    QnResourcePropertyAdaptor<QString>* m_cloudSystemIDAdaptor;
    QnResourcePropertyAdaptor<QString>* m_cloudAuthKeyAdaptor;

    // misc adaptors
    QnResourcePropertyAdaptor<QString>* m_systemNameAdaptor;
    QnResourcePropertyAdaptor<bool>* m_arecontRtspEnabledAdaptor;
    QnResourcePropertyAdaptor<bool>* m_newSystemAdaptor;
    QnResourcePropertyAdaptor<QString>* m_cloudHostAdaptor;

    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizeBytes;
    QnResourcePropertyAdaptor<int>* m_maxRecorderQueueSizePackets;

    AdaptorList m_allAdaptors;

    mutable QnMutex m_mutex;
    QnUserResourcePtr m_admin;
};

#define qnGlobalSettings QnGlobalSettings::instance()
