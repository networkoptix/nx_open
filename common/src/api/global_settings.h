#ifndef QN_GLOBAL_SETTINGS_H
#define QN_GLOBAL_SETTINGS_H

#include <utils/thread/mutex.h>
#include <QtCore/QObject>

#include <nx_ec/data/api_fwd.h>

#include <utils/common/singleton.h>
#include <utils/common/connective.h>
#include <utils/email/email_fwd.h>
#include <utils/common/ldap_fwd.h>

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

    void synchronizeNow();
    QnUserResourcePtr getAdminUser();

    bool isUpdateNotificationsEnabled() const;
    void setUpdateNotificationsEnabled(bool updateNotificationsEnabled);

    Qn::CameraBackupQualities defaultBackupQuality() const;
    void setDefauldBackupQuality(Qn::CameraBackupQualities value);
signals:
    void disabledVendorsChanged();
    void auditTrailEnableChanged();
    void cameraSettingsOptimizationChanged();
    void serverAutoDiscoveryChanged();
    void emailSettingsChanged();
    void ldapSettingsChanged();

private:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QnResourcePropertyAdaptor<bool> *m_cameraSettingsOptimizationAdaptor;
    QnResourcePropertyAdaptor<bool> *m_auditTrailEnabledAdaptor;
    QnResourcePropertyAdaptor<QString> *m_disabledVendorsAdaptor;
    QnResourcePropertyAdaptor<bool> *m_serverAutoDiscoveryEnabledAdaptor;
    QnResourcePropertyAdaptor<bool> *m_updateNotificationsEnabledAdaptor;
    QnResourcePropertyAdaptor<Qn::CameraBackupQualities> *m_defaultBackupQualityAdaptor;

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


    QList<QnAbstractResourcePropertyAdaptor*> m_allAdaptors;

    mutable QnMutex m_mutex;
    QnUserResourcePtr m_admin;
};

#define qnGlobalSettings QnGlobalSettings::instance()

#endif // QN_GLOBAL_SETTINGS_H
