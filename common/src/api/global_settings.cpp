#include "global_settings.h"

#include <cassert>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include "resource_property_adaptor.h"

#include <utils/common/app_info.h>
#include <utils/email/email.h>
#include <utils/common/ldap.h>

#include <nx_ec/data/api_resource_data.h>


namespace {
    QSet<QString> parseDisabledVendors(QString disabledVendors) {
        QStringList disabledVendorList;
        if (disabledVendors.contains(lit(";")))
            disabledVendorList = disabledVendors.split(lit(";"));
        else
            disabledVendorList = disabledVendors.split(lit(" "));

        QStringList updatedVendorList;        
        for (int i = 0; i < disabledVendorList.size(); ++i) {
            if (!disabledVendorList[i].trimmed().isEmpty()) {
                updatedVendorList << disabledVendorList[i].trimmed();
            }
        }

        return updatedVendorList.toSet();
    }

    const QString nameDisabledVendors(lit("disabledVendors"));
    const QString nameCameraSettingsOptimization(lit("cameraSettingsOptimization"));
    const QString nameAuditTrailEnabled(lit("auditTrailEnabled"));
    const QString nameHost(lit("smtpHost"));
    const QString namePort(lit("smtpPort"));
    const QString nameUser(lit("smtpUser"));
    const QString namePassword(lit("smptPassword"));
    const QString nameConnectionType(lit("smtpConnectionType"));
    const QString nameSimple(lit("smtpSimple"));
    const QString nameTimeout(lit("smtpTimeout"));
    const QString nameFrom(lit("emailFrom"));
    const QString nameSignature(lit("emailSignature"));
    const QString nameSupportEmail(lit("emailSupportEmail"));
    const QString nameUpdateNotificationsEnabled(lit("updateNotificationsEnabled"));
    const QString nameTimeSynchronizationEnabled(lit("timeSynchronizationEnabled"));
    const QString nameServerAutoDiscoveryEnabled(lit("serverAutoDiscoveryEnabled"));

    const QString ldapUri(lit("ldapUri"));
    const QString ldapAdminDn(lit("ldapAdminDn"));
    const QString ldapAdminPassword(lit("ldapAdminPassword"));
    const QString ldapSearchBase(lit("ldapSearchBase"));
    const QString ldapSearchFilter(lit("ldapSearchFilter"));

    const QString kEc2ConnectionKeepAliveTimeout(lit("ec2ConnectionKeepAliveTimeoutSec"));
    const int kEc2ConnectionKeepAliveTimeoutDefault = 5;
    const QString kEc2KeepAliveProbeCount(lit("ec2KeepAliveProbeCount"));
    const int kEc2KeepAliveProbeCountDefault = 3;
    const QString kEc2AliveUpdateInterval(lit("ec2AliveUpdateIntervalSec"));
    const int kEc2AliveUpdateIntervalDefault = 60;
    const QString kServerDiscoveryPingTimeout(lit("serverDiscoveryPingTimeoutSec"));
    const int kServerDiscoveryPingTimeoutDefault = 60;

    const QString kTakeCameraOwnershipWithoutLock(lit("takeCameraOwnershipWithoutLock"));
    const int kTakeCameraOwnershipWithoutLockDefault = false;
}

QnGlobalSettings::QnGlobalSettings(QObject *parent): 
    base_type(parent)
{
    assert(qnResPool);
    
    QString defaultSupportLink = QnAppInfo::supportLink();
    if (defaultSupportLink.isEmpty())
        defaultSupportLink = QnAppInfo::supportEmailAddress();

    m_disabledVendorsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(nameDisabledVendors, QString(), this);
    m_cameraSettingsOptimizationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(nameCameraSettingsOptimization, true, this);
    m_auditTrailEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(nameAuditTrailEnabled, true, this);
    m_serverAutoDiscoveryEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(nameServerAutoDiscoveryEnabled, true, this);
    m_updateNotificationsEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(nameUpdateNotificationsEnabled, true, this);
    m_serverAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(nameHost, QString(), this);
    m_fromAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(nameFrom, QString(), this);
    m_userAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(nameUser, QString(), this);
    m_passwordAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(namePassword, QString(), this);
    m_signatureAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(nameSignature, QString(), this);
    m_supportLinkAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(nameSupportEmail, defaultSupportLink, this);
    m_connectionTypeAdaptor = new  QnLexicalResourcePropertyAdaptor<QnEmail::ConnectionType>(nameConnectionType, QnEmail::Unsecure, this);
    m_portAdaptor = new QnLexicalResourcePropertyAdaptor<int>(namePort, 0, this);
    m_timeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(nameTimeout, QnEmailSettings::defaultTimeoutSec(), this);
    m_simpleAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(nameSimple, true, this);

    m_ldapUriAdaptor = new QnLexicalResourcePropertyAdaptor<QUrl>(ldapUri, QUrl(), this);
    m_ldapAdminDnAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapAdminDn, QString(), this);
    m_ldapAdminPasswordAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapAdminPassword, QString(), this);
    m_ldapSearchBaseAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapSearchBase, QString(), this);
    m_ldapSearchFilterAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapSearchFilter, QString(), this);

    QList<QnAbstractResourcePropertyAdaptor*> ec2Adaptors;
    m_ec2ConnectionKeepAliveTimeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kEc2ConnectionKeepAliveTimeout,
        kEc2ConnectionKeepAliveTimeoutDefault,
        this);
    ec2Adaptors << m_ec2ConnectionKeepAliveTimeoutAdaptor;
    m_ec2KeepAliveProbeCountAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kEc2KeepAliveProbeCount,
        kEc2KeepAliveProbeCountDefault,
        this);
    ec2Adaptors << m_ec2KeepAliveProbeCountAdaptor;
    m_ec2AliveUpdateIntervalAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kEc2AliveUpdateInterval,
        kEc2AliveUpdateIntervalDefault,
        this);
    ec2Adaptors << m_ec2AliveUpdateIntervalAdaptor;
    m_serverDiscoveryPingTimeout = new QnLexicalResourcePropertyAdaptor<int>(
        kServerDiscoveryPingTimeout,
        kServerDiscoveryPingTimeoutDefault,
        this);
    ec2Adaptors << m_serverDiscoveryPingTimeout;
    m_timeSynchronizationEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        nameTimeSynchronizationEnabled,
        true,
        this);
    ec2Adaptors << m_timeSynchronizationEnabledAdaptor;
    m_takeCameraOwnershipWithoutLock = new QnLexicalResourcePropertyAdaptor<bool>(
        kTakeCameraOwnershipWithoutLock,
        kTakeCameraOwnershipWithoutLockDefault,
        this);
    ec2Adaptors << m_takeCameraOwnershipWithoutLock;

    QList<QnAbstractResourcePropertyAdaptor*> emailAdaptors;
    emailAdaptors
        << m_serverAdaptor
        << m_fromAdaptor
        << m_userAdaptor
        << m_passwordAdaptor
        << m_signatureAdaptor
        << m_supportLinkAdaptor
        << m_connectionTypeAdaptor
        << m_portAdaptor
        << m_timeoutAdaptor
        << m_simpleAdaptor
        ;

    QList<QnAbstractResourcePropertyAdaptor*> ldapAdaptors;
    ldapAdaptors
        << m_ldapUriAdaptor
        << m_ldapAdminDnAdaptor
        << m_ldapAdminPasswordAdaptor
        << m_ldapSearchBaseAdaptor
        << m_ldapSearchFilterAdaptor
        ;

    m_allAdaptors 
        << m_disabledVendorsAdaptor
        << m_cameraSettingsOptimizationAdaptor
        << m_serverAutoDiscoveryEnabledAdaptor
        << m_updateNotificationsEnabledAdaptor
        << emailAdaptors
        << ldapAdaptors
        << m_auditTrailEnabledAdaptor
        << ec2Adaptors;

    connect(m_disabledVendorsAdaptor,               &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::disabledVendorsChanged,              Qt::QueuedConnection);
    connect(m_auditTrailEnabledAdaptor,             &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::auditTrailEnableChanged,             Qt::QueuedConnection);
    connect(m_cameraSettingsOptimizationAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::cameraSettingsOptimizationChanged,   Qt::QueuedConnection);
    connect(m_serverAutoDiscoveryEnabledAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::serverAutoDiscoveryChanged,          Qt::QueuedConnection);

    for(QnAbstractResourcePropertyAdaptor* adaptor: emailAdaptors)
        connect(adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::emailSettingsChanged, Qt::QueuedConnection);

    for(QnAbstractResourcePropertyAdaptor* adaptor: ldapAdaptors)
        connect(adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::ldapSettingsChanged, Qt::QueuedConnection);

    for(auto adaptor: ec2Adaptors)
        connect(
            adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
            this, &QnGlobalSettings::ec2ConnectionSettingsChanged,
            Qt::QueuedConnection);

    connect(qnResPool,                              &QnResourcePool::resourceAdded,                     this,   &QnGlobalSettings::at_resourcePool_resourceAdded);
    connect(qnResPool,                              &QnResourcePool::resourceRemoved,                   this,   &QnGlobalSettings::at_resourcePool_resourceRemoved);
    for(const QnResourcePtr &resource: qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnGlobalSettings::~QnGlobalSettings() {
    disconnect(qnResPool, NULL, this, NULL);
    if(m_admin)
        at_resourcePool_resourceRemoved(m_admin);
}

QString QnGlobalSettings::disabledVendors() const {
    return m_disabledVendorsAdaptor->value();
}

QSet<QString> QnGlobalSettings::disabledVendorsSet() const {
    return parseDisabledVendors(disabledVendors());
}

void QnGlobalSettings::setDisabledVendors(QString disabledVendors) {
    m_disabledVendorsAdaptor->setValue(disabledVendors);
}

bool QnGlobalSettings::isCameraSettingsOptimizationEnabled() const {
    return m_cameraSettingsOptimizationAdaptor->value();
}

void QnGlobalSettings::setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled) {
    m_cameraSettingsOptimizationAdaptor->setValue(cameraSettingsOptimizationEnabled);
}

bool QnGlobalSettings::isAuditTrailEnabled() const
{
    return m_auditTrailEnabledAdaptor->value();
}

void QnGlobalSettings::setAuditTrailEnabled(bool value)
{
    m_auditTrailEnabledAdaptor->setValue(value);
}

bool QnGlobalSettings::isServerAutoDiscoveryEnabled() const {
    return m_serverAutoDiscoveryEnabledAdaptor->value();
}

void QnGlobalSettings::setServerAutoDiscoveryEnabled(bool enabled) {
    m_serverAutoDiscoveryEnabledAdaptor->setValue(enabled);
}

void QnGlobalSettings::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if(m_admin)
        return;

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    if(!user->isAdmin())
        return;

    QMutexLocker locker(&m_mutex);

    m_admin = user;
    for (QnAbstractResourcePropertyAdaptor* adaptor: m_allAdaptors)
        adaptor->setResource(user);
}

void QnGlobalSettings::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if (!m_admin || resource != m_admin)
        return;

    QMutexLocker locker(&m_mutex);
    m_admin.reset();

    for (QnAbstractResourcePropertyAdaptor* adaptor: m_allAdaptors)
        adaptor->setResource(QnResourcePtr());
}

QnLdapSettings QnGlobalSettings::ldapSettings() const {
    QnLdapSettings result;
    result.uri = m_ldapUriAdaptor->value();
    result.adminDn = m_ldapAdminDnAdaptor->value();
    result.adminPassword = m_ldapAdminPasswordAdaptor->value();
    result.searchBase = m_ldapSearchBaseAdaptor->value();
    result.searchFilter = m_ldapSearchFilterAdaptor->value();
    return result;
}

void QnGlobalSettings::setLdapSettings(const QnLdapSettings &settings) {
    m_ldapUriAdaptor->setValue(settings.uri);
    m_ldapAdminDnAdaptor->setValue(settings.adminDn);
    m_ldapAdminPasswordAdaptor->setValue(settings.adminPassword);
    m_ldapSearchBaseAdaptor->setValue(settings.searchBase);
    m_ldapSearchFilterAdaptor->setValue(settings.searchFilter);
}

QnEmailSettings QnGlobalSettings::emailSettings() const {
    QnEmailSettings result;
    result.server = m_serverAdaptor->value();
    result.email = m_fromAdaptor->value();
    result.port = m_portAdaptor->value();
    result.user = m_userAdaptor->value();
    result.password = m_passwordAdaptor->value();
    result.connectionType = m_connectionTypeAdaptor->value();
    result.signature = m_signatureAdaptor->value();
    result.supportEmail = m_supportLinkAdaptor->value();
    result.simple = m_simpleAdaptor->value();
    result.timeout = m_timeoutAdaptor->value();

    /*
     * VMS-1055 - default email changed to link.
     * We are checking if the value is not overridden and replacing it by the updated one. 
     */
    if (result.supportEmail == QnAppInfo::supportEmailAddress() && !QnAppInfo::supportLink().isEmpty()) {
        result.supportEmail = QnAppInfo::supportLink();
    }

    return result;
}

void QnGlobalSettings::setEmailSettings(const QnEmailSettings &settings) {
    m_serverAdaptor->setValue(settings.server);
    m_fromAdaptor->setValue(settings.email);
    m_portAdaptor->setValue(settings.port == QnEmailSettings::defaultPort(settings.connectionType) ? 0 : settings.port);
    m_userAdaptor->setValue(settings.user);
    m_passwordAdaptor->setValue(settings.isValid() ? settings.password : QString());
    m_connectionTypeAdaptor->setValue(settings.connectionType);
    m_signatureAdaptor->setValue(settings.signature);
    m_supportLinkAdaptor->setValue(settings.supportEmail);
    m_simpleAdaptor->setValue(settings.simple);
    m_timeoutAdaptor->setValue(settings.timeout);   
}

void QnGlobalSettings::synchronizeNow() {
    for (QnAbstractResourcePropertyAdaptor* adaptor: m_allAdaptors)
        adaptor->synchronizeNow();
}

QnUserResourcePtr QnGlobalSettings::getAdminUser() {
    return m_admin;
}

bool QnGlobalSettings::isUpdateNotificationsEnabled() const {
    return m_updateNotificationsEnabledAdaptor->value();
}

void QnGlobalSettings::setUpdateNotificationsEnabled(bool updateNotificationsEnabled) {
    m_updateNotificationsEnabledAdaptor->setValue(updateNotificationsEnabled);
}

std::chrono::seconds QnGlobalSettings::connectionKeepAliveTimeout() const
{
    return std::chrono::seconds(m_ec2ConnectionKeepAliveTimeoutAdaptor->value());
}

void QnGlobalSettings::setConnectionKeepAliveTimeout(std::chrono::seconds newTimeout)
{
    m_ec2ConnectionKeepAliveTimeoutAdaptor->setValue(newTimeout.count());
}

int QnGlobalSettings::keepAliveProbeCount() const
{
    return m_ec2KeepAliveProbeCountAdaptor->value();
}

void QnGlobalSettings::setKeepAliveProbeCount(int newProbeCount)
{
    m_ec2KeepAliveProbeCountAdaptor->setValue(newProbeCount);
}

std::chrono::seconds QnGlobalSettings::aliveUpdateInterval() const
{
    return std::chrono::seconds(m_ec2AliveUpdateIntervalAdaptor->value());
}

void QnGlobalSettings::setAliveUpdateInterval(std::chrono::seconds newInterval) const
{
    m_ec2AliveUpdateIntervalAdaptor->setValue(newInterval.count());
}

std::chrono::seconds QnGlobalSettings::serverDiscoveryPingTimeout() const
{
    return std::chrono::seconds(m_serverDiscoveryPingTimeout->value());
}

void QnGlobalSettings::setServerDiscoveryPingTimeout(std::chrono::seconds newInterval) const
{
    m_serverDiscoveryPingTimeout->setValue(newInterval.count());
}

std::chrono::seconds QnGlobalSettings::serverDiscoveryAliveCheckTimeout() const
{
    return connectionKeepAliveTimeout() * 3;   //3 is here to keep same values as before by default
}

bool QnGlobalSettings::isTimeSynchronizationEnabled() const
{
    return m_timeSynchronizationEnabledAdaptor->value();
}

bool QnGlobalSettings::takeCameraOwnershipWithoutLock() const
{
    return m_takeCameraOwnershipWithoutLock->value();
}

const QList<QnAbstractResourcePropertyAdaptor*>& QnGlobalSettings::allSettings() const
{
    return m_allAdaptors;
}
