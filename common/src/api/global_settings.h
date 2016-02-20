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

#include <core/resource/resource_fwd.h>

class QnAbstractResourcePropertyAdaptor;

template<class T>
class QnResourcePropertyAdaptor;

class QnGlobalSettings: public Connective<QObject>, public Singleton<QnGlobalSettings> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnGlobalSettings(QObject *parent = NULL);
    virtual ~QnGlobalSettings();

    void synchronizeNow();


    QSet<QString> disabledVendorsSet() const;
    QString disabledVendors() const;
    void setDisabledVendors(QString disabledVendors);

    bool isCameraSettingsOptimizationEnabled() const;
    void setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled);

    bool isAuditTrailEnabled() const;
    void setAuditTrailEnabled(bool value);

    bool isServerAutoDiscoveryEnabled() const;
    void setServerAutoDiscoveryEnabled(bool enabled);

    bool isCrossdomainXmlEnabled() const;
    void setCrossdomainXmlEnabled(bool enabled);

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

    bool arecontRtspEnabled() const;
    void setArecontRtspEnabled(bool newVal) const;

    /*!
        \a QnAbstractResourcePropertyAdaptor class methods are thread-safe
        \note returned list is not changed during \a QnGlobalSettings instance life-time
    */
    const QList<QnAbstractResourcePropertyAdaptor*>& allSettings() const;

signals:
    void disabledVendorsChanged();
    void auditTrailEnableChanged();
    void cameraSettingsOptimizationChanged();
    void serverAutoDiscoveryChanged();
    void emailSettingsChanged();
    void ldapSettingsChanged();
    void statisticsAllowedChanged();
    void updateNotificationsChanged();
    void ec2ConnectionSettingsChanged();

private:
    typedef QList<QnAbstractResourcePropertyAdaptor*> AdaptorList;

    AdaptorList initEmailAdaptors();
    AdaptorList initLdapAdaptors();
    AdaptorList initMiscAdaptors();

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QnResourcePropertyAdaptor<bool> *m_cameraSettingsOptimizationAdaptor;
    QnResourcePropertyAdaptor<bool> *m_auditTrailEnabledAdaptor;
    QnResourcePropertyAdaptor<QString> *m_disabledVendorsAdaptor;
    QnResourcePropertyAdaptor<bool> *m_serverAutoDiscoveryEnabledAdaptor;
    QnResourcePropertyAdaptor<bool> *m_crossdomainXmlEnabledAdaptor;
    QnResourcePropertyAdaptor<bool> *m_updateNotificationsEnabledAdaptor;
    QnResourcePropertyAdaptor<bool> *m_timeSynchronizationEnabledAdaptor;
    QnResourcePropertyAdaptor<Qn::CameraBackupQualities> *m_backupQualitiesAdaptor;
    QnResourcePropertyAdaptor<bool> *m_backupNewCamerasByDefaultAdaptor;
    QnResourcePropertyAdaptor<QnOptionalBool> *m_statisticsAllowedAdaptor;

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
    QnResourcePropertyAdaptor<int>* m_serverDiscoveryPingTimeout;

    QnResourcePropertyAdaptor<bool>* m_arecontRtspEnabled;

    AdaptorList m_allAdaptors;

    mutable QnMutex m_mutex;
    QnUserResourcePtr m_admin;
};

#define qnGlobalSettings QnGlobalSettings::instance()
