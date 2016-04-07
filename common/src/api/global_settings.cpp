#include "global_settings.h"

#include <cassert>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>

#include "resource_property_adaptor.h"

#include <utils/common/app_info.h>
#include <utils/email/email.h>
#include <utils/common/ldap.h>

#include <nx_ec/data/api_resource_data.h>


namespace
{
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

    const QString kNameDisabledVendors(lit("disabledVendors"));
    const QString kNameCameraSettingsOptimization(lit("cameraSettingsOptimization"));
    const QString kNameAuditTrailEnabled(lit("auditTrailEnabled"));
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
    const QString kNameServerAutoDiscoveryEnabled(lit("serverAutoDiscoveryEnabled"));
    const QString kNameBackupQualities(lit("backupQualities"));
    const QString kNameBackupNewCamerasByDefault(lit("backupNewCamerasByDefault"));
    const QString kNameCrossdomainEnabled(lit("crossdomainEnabled"));
    const QString kNameNewSystem(lit("newSystem"));

    const QString kNameStatisticsAllowed(lit("statisticsAllowed"));
    const QString kNameStatisticsReportLastTime(lit("statisticsReportLastTime"));
    const QString kNameStatisticsReportLastNumber(lit("statisticsReportLastNumber"));
    const QString kNameStatisticsReportTimeCycle(lit("statisticsReportTimeCycle"));
    const QString kNameSystemId(lit("systemId"));
    const QString kNameSystemNameForId(lit("systemNameForId"));
    const QString kNameStatisticsReportServerApi(lit("statisticsReportServerApi"));

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

    const QString kArecontRtspEnabled(lit("arecontRtspEnabled"));
    const bool kArecontRtspEnabledDefault = false;
    const QString kProxyConnectTimeout(lit("proxyConnectTimeoutSec"));
    const int kProxyConnectTimeoutDefault = 5;
}

QnGlobalSettings::QnGlobalSettings(QObject *parent):
    base_type(parent)
{
    NX_ASSERT(qnResPool);

    m_allAdaptors
        << initEmailAdaptors()
        << initLdapAdaptors()
        << initStaticticsAdaptors()
        << initConnectionAdaptors()
        << initCloudAdaptors()
        << initMiscAdaptors()
        ;

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


bool QnGlobalSettings::isInitialized() const
{
    return !m_admin.isNull();
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initEmailAdaptors() {
    QString defaultSupportLink = QnAppInfo::supportLink();
    if (defaultSupportLink.isEmpty())
        defaultSupportLink = QnAppInfo::supportEmailAddress();

    m_serverAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameHost, QString(), this);
    m_fromAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameFrom, QString(), this);
    m_userAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameUser, QString(), this);
    m_passwordAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNamePassword, QString(), this);
    m_signatureAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameSignature, QString(), this);
    m_supportLinkAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameSupportEmail, defaultSupportLink, this);
    m_connectionTypeAdaptor = new  QnLexicalResourcePropertyAdaptor<QnEmail::ConnectionType>(kNameConnectionType, QnEmail::Unsecure, this);
    m_portAdaptor = new QnLexicalResourcePropertyAdaptor<int>(kNamePort, 0, this);
    m_timeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(kNameTimeout, QnEmailSettings::defaultTimeoutSec(), this);
    m_simpleAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameSimple, true, this);

    QnGlobalSettings::AdaptorList result;
    result
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

    for(QnAbstractResourcePropertyAdaptor* adaptor: result)
        connect(adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::emailSettingsChanged, Qt::QueuedConnection);

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initLdapAdaptors() {
    m_ldapUriAdaptor = new QnLexicalResourcePropertyAdaptor<QUrl>(ldapUri, QUrl(), this);
    m_ldapAdminDnAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapAdminDn, QString(), this);
    m_ldapAdminPasswordAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapAdminPassword, QString(), this);
    m_ldapSearchBaseAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapSearchBase, QString(), this);
    m_ldapSearchFilterAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(ldapSearchFilter, QString(), this);

    QnGlobalSettings::AdaptorList result;
    result
        << m_ldapUriAdaptor
        << m_ldapAdminDnAdaptor
        << m_ldapAdminPasswordAdaptor
        << m_ldapSearchBaseAdaptor
        << m_ldapSearchFilterAdaptor
        ;

    for(QnAbstractResourcePropertyAdaptor* adaptor: result)
        connect(adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::ldapSettingsChanged, Qt::QueuedConnection);

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initStaticticsAdaptors()
{
    m_statisticsAllowedAdaptor = new QnLexicalResourcePropertyAdaptor<QnOptionalBool>(kNameStatisticsAllowed, QnOptionalBool(), this);
    m_statisticsReportLastTimeAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameStatisticsReportLastTime, QString(), this);
    m_statisticsReportLastNumberAdaptor = new QnLexicalResourcePropertyAdaptor<int>(kNameStatisticsReportLastNumber, 0, this);
    m_statisticsReportTimeCycleAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameStatisticsReportTimeCycle, QString(), this);
    m_systemIdAdaptor = new QnLexicalResourcePropertyAdaptor<QnUuid>(kNameSystemId, QnUuid(), this);
    m_systemNameForIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameSystemNameForId, QString(), this);
    m_statisticsReportServerApiAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameStatisticsReportServerApi, QString(), this);

    connect(m_statisticsAllowedAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this, &QnGlobalSettings::statisticsAllowedChanged, Qt::QueuedConnection);

    QnGlobalSettings::AdaptorList result;
    result
        << m_statisticsAllowedAdaptor
        << m_statisticsReportLastTimeAdaptor
        << m_statisticsReportLastNumberAdaptor
        << m_statisticsReportTimeCycleAdaptor
        << m_systemIdAdaptor
        << m_systemNameForIdAdaptor
        << m_statisticsReportServerApiAdaptor
        ;

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initConnectionAdaptors()
{
    AdaptorList ec2Adaptors;
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
    m_serverDiscoveryPingTimeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kServerDiscoveryPingTimeout,
        kServerDiscoveryPingTimeoutDefault,
        this);
    ec2Adaptors << m_serverDiscoveryPingTimeoutAdaptor;
    m_timeSynchronizationEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameTimeSynchronizationEnabled,
        true,
        this);
    ec2Adaptors << m_timeSynchronizationEnabledAdaptor;
    m_proxyConnectTimeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kProxyConnectTimeout,
        kProxyConnectTimeoutDefault,
        this);
    ec2Adaptors << m_proxyConnectTimeoutAdaptor;

    for (auto adaptor : ec2Adaptors)
        connect(
            adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
            this, &QnGlobalSettings::ec2ConnectionSettingsChanged,
            Qt::QueuedConnection);

    return ec2Adaptors;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initCloudAdaptors()
{
    m_cloudAccountNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameCloudAccountName, QString(), this);
    m_cloudSystemIDAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameCloudSystemID, QString(), this);
    m_cloudAuthKeyAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameCloudAuthKey, QString(), this);

    QnGlobalSettings::AdaptorList result;
    result
        << m_cloudAccountNameAdaptor
        << m_cloudSystemIDAdaptor
        << m_cloudAuthKeyAdaptor
        ;

    for (QnAbstractResourcePropertyAdaptor* adaptor : result)
        connect(adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this, &QnGlobalSettings::cloudSettingsChanged, Qt::QueuedConnection);

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initMiscAdaptors()
{
    m_disabledVendorsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameDisabledVendors, QString(), this);
    m_cameraSettingsOptimizationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameCameraSettingsOptimization, true, this);
    m_auditTrailEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameAuditTrailEnabled, true, this);
    m_serverAutoDiscoveryEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameServerAutoDiscoveryEnabled, true, this);
    m_updateNotificationsEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameUpdateNotificationsEnabled, true, this);
    m_backupQualitiesAdaptor = new QnLexicalResourcePropertyAdaptor<Qn::CameraBackupQualities>(kNameBackupQualities, Qn::CameraBackup_Both, this);
    m_backupNewCamerasByDefaultAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameBackupNewCamerasByDefault, false, this);
	m_crossdomainXmlEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameCrossdomainEnabled, true, this);
    m_upnpPortMappingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameUpnpPortMappingEnabled, true, this);
    m_newSystemAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameNewSystem, false, this);

    m_arecontRtspEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kArecontRtspEnabled,
        kArecontRtspEnabledDefault,
        this);

    connect(m_disabledVendorsAdaptor,               &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::disabledVendorsChanged,              Qt::QueuedConnection);
    connect(m_auditTrailEnabledAdaptor,             &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::auditTrailEnableChanged,             Qt::QueuedConnection);
    connect(m_cameraSettingsOptimizationAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::cameraSettingsOptimizationChanged,   Qt::QueuedConnection);
    connect(m_serverAutoDiscoveryEnabledAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::serverAutoDiscoveryChanged,          Qt::QueuedConnection);
    connect(m_updateNotificationsEnabledAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::updateNotificationsChanged,          Qt::QueuedConnection);
    connect(m_upnpPortMappingEnabledAdaptor,        &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::upnpPortMappingEnabledChanged,       Qt::QueuedConnection);
    connect(m_newSystemAdaptor,                     &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::newSystemChanged,                    Qt::QueuedConnection);

    QnGlobalSettings::AdaptorList result;
    result
        << m_disabledVendorsAdaptor
        << m_cameraSettingsOptimizationAdaptor
        << m_auditTrailEnabledAdaptor
        << m_serverAutoDiscoveryEnabledAdaptor
        << m_updateNotificationsEnabledAdaptor
        << m_backupQualitiesAdaptor
        << m_backupNewCamerasByDefaultAdaptor
		<< m_crossdomainXmlEnabledAdaptor
        << m_upnpPortMappingEnabledAdaptor
        << m_newSystemAdaptor
        << m_arecontRtspEnabledAdaptor
        ;

    return result;
}

QString QnGlobalSettings::disabledVendors() const
{
    return m_disabledVendorsAdaptor->value();
}

QSet<QString> QnGlobalSettings::disabledVendorsSet() const
{
    return parseDisabledVendors(disabledVendors());
}

void QnGlobalSettings::setDisabledVendors(QString disabledVendors)
{
    m_disabledVendorsAdaptor->setValue(disabledVendors);
}

bool QnGlobalSettings::isCameraSettingsOptimizationEnabled() const
{
    return m_cameraSettingsOptimizationAdaptor->value();
}

void QnGlobalSettings::setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled)
{
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

void QnGlobalSettings::setServerAutoDiscoveryEnabled(bool enabled)
{
    m_serverAutoDiscoveryEnabledAdaptor->setValue(enabled);
}

bool QnGlobalSettings::isCrossdomainXmlEnabled() const
{
    return m_crossdomainXmlEnabledAdaptor->value();
}

void QnGlobalSettings::setCrossdomainXmlEnabled(bool enabled)
{
    m_crossdomainXmlEnabledAdaptor->setValue(enabled);
}

void QnGlobalSettings::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if(m_admin)
        return;

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    if(!user->isAdmin())
        return;

    {
        QnMutexLocker locker(&m_mutex);

        m_admin = user;
        for (QnAbstractResourcePropertyAdaptor* adaptor : m_allAdaptors)
            adaptor->setResource(user);
    }
    emit initialized();
}

void QnGlobalSettings::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    if (!m_admin || resource != m_admin)
        return;

    QnMutexLocker locker( &m_mutex );
    m_admin.reset();

    for (QnAbstractResourcePropertyAdaptor* adaptor: m_allAdaptors)
        adaptor->setResource(QnResourcePtr());
}

QnLdapSettings QnGlobalSettings::ldapSettings() const
{
    QnLdapSettings result;
    result.uri = m_ldapUriAdaptor->value();
    result.adminDn = m_ldapAdminDnAdaptor->value();
    result.adminPassword = m_ldapAdminPasswordAdaptor->value();
    result.searchBase = m_ldapSearchBaseAdaptor->value();
    result.searchFilter = m_ldapSearchFilterAdaptor->value();
    return result;
}

void QnGlobalSettings::setLdapSettings(const QnLdapSettings &settings)
{
    m_ldapUriAdaptor->setValue(settings.uri);
    m_ldapAdminDnAdaptor->setValue(settings.adminDn);
    m_ldapAdminPasswordAdaptor->setValue(settings.adminPassword);
    m_ldapSearchBaseAdaptor->setValue(settings.searchBase);
    m_ldapSearchFilterAdaptor->setValue(settings.searchFilter);
}

QnEmailSettings QnGlobalSettings::emailSettings() const
{
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
    if (result.supportEmail == QnAppInfo::supportEmailAddress() && !QnAppInfo::supportLink().isEmpty())
    {
        result.supportEmail = QnAppInfo::supportLink();
    }

    return result;
}

void QnGlobalSettings::setEmailSettings(const QnEmailSettings &settings)
{
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

void QnGlobalSettings::synchronizeNow()
{
    for (QnAbstractResourcePropertyAdaptor* adaptor: m_allAdaptors)
        adaptor->saveToResource();

    QnMutexLocker locker(&m_mutex);
    NX_ASSERT(m_admin, Q_FUNC_INFO, "Invalid sync state");
    if (!m_admin)
        return;
    propertyDictionary->saveParamsAsync(m_admin->getId());
}

bool QnGlobalSettings::synchronizeNowSync()
{
    for (QnAbstractResourcePropertyAdaptor* adaptor : m_allAdaptors)
        adaptor->saveToResource();

    QnMutexLocker locker(&m_mutex);
    NX_ASSERT(m_admin, Q_FUNC_INFO, "Invalid sync state");
    if (!m_admin)
        return false;
    return propertyDictionary->saveParams(m_admin->getId());
}

bool QnGlobalSettings::isUpdateNotificationsEnabled() const
{
    return m_updateNotificationsEnabledAdaptor->value();
}

void QnGlobalSettings::setUpdateNotificationsEnabled(bool updateNotificationsEnabled)
{
    m_updateNotificationsEnabledAdaptor->setValue(updateNotificationsEnabled);
}

Qn::CameraBackupQualities QnGlobalSettings::backupQualities() const
{
    return m_backupQualitiesAdaptor->value();
}

void QnGlobalSettings::setBackupQualities( Qn::CameraBackupQualities value )
{
    m_backupQualitiesAdaptor->setValue(value);
}

bool QnGlobalSettings::backupNewCamerasByDefault() const
{
    return m_backupNewCamerasByDefaultAdaptor->value();
}

void QnGlobalSettings::setBackupNewCamerasByDefault( bool value )
{
    m_backupNewCamerasByDefaultAdaptor->setValue(value);
}

bool QnGlobalSettings::isStatisticsAllowedDefined() const
{
    return m_statisticsAllowedAdaptor->value().isDefined();
}

bool QnGlobalSettings::isStatisticsAllowed() const
{
    /* Undefined value means we are allowed to send statistics. */
    return !m_statisticsAllowedAdaptor->value().isDefined()
        || m_statisticsAllowedAdaptor->value().value();
}

void QnGlobalSettings::setStatisticsAllowed( bool value )
{
    m_statisticsAllowedAdaptor->setValue(QnOptionalBool(value));
}

QDateTime QnGlobalSettings::statisticsReportLastTime() const
{
    return QDateTime::fromString(m_statisticsReportLastTimeAdaptor->value(), Qt::ISODate);
}

void QnGlobalSettings::setStatisticsReportLastTime(const QDateTime& value)
{
    m_statisticsReportLastTimeAdaptor->setValue(value.toString(Qt::ISODate));
}

int QnGlobalSettings::statisticsReportLastNumber() const
{
    return m_statisticsReportLastNumberAdaptor->value();
}

void QnGlobalSettings::setStatisticsReportLastNumber(int value)
{
    m_statisticsReportLastNumberAdaptor->setValue(value);
}

QString QnGlobalSettings::statisticsReportTimeCycle() const
{
    return m_statisticsReportTimeCycleAdaptor->value();
}

void QnGlobalSettings::setStatisticsReportTimeCycle(const QString& value)
{
    m_statisticsReportTimeCycleAdaptor->setValue(value);
}

const QString QnGlobalSettings::kNameUpnpPortMappingEnabled(lit("upnpPortMappingEnabled"));

bool QnGlobalSettings::isUpnpPortMappingEnabled() const
{
    return m_upnpPortMappingEnabledAdaptor->value();
}

void QnGlobalSettings::setUpnpPortMappingEnabled(bool value)
{
    m_upnpPortMappingEnabledAdaptor->setValue(value);
}

QnUuid QnGlobalSettings::systemId() const
{
    return m_systemIdAdaptor->value();
}

void QnGlobalSettings::setSystemId(const QnUuid &value)
{
    m_systemIdAdaptor->setValue(value);
}

QString QnGlobalSettings::systemNameForId() const
{
    return m_systemNameForIdAdaptor->value();
}

void QnGlobalSettings::setSystemNameForId(const QString &value)
{
    m_systemNameForIdAdaptor->setValue(value);
}

QString QnGlobalSettings::statisticsReportServerApi() const
{
    return m_statisticsReportServerApiAdaptor->value();
}

void QnGlobalSettings::setStatisticsReportServerApi(const QString &value)
{
    m_statisticsReportServerApiAdaptor->setValue(value);
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
    return std::chrono::seconds(m_serverDiscoveryPingTimeoutAdaptor->value());
}

void QnGlobalSettings::setServerDiscoveryPingTimeout(std::chrono::seconds newInterval) const
{
    m_serverDiscoveryPingTimeoutAdaptor->setValue(newInterval.count());
}

std::chrono::seconds QnGlobalSettings::serverDiscoveryAliveCheckTimeout() const
{
    return connectionKeepAliveTimeout() * 3;   //3 is here to keep same values as before by default
}

bool QnGlobalSettings::isTimeSynchronizationEnabled() const
{
    return m_timeSynchronizationEnabledAdaptor->value();
}

const QString QnGlobalSettings::kNameCloudAccountName(lit("cloudAccountName"));

QString QnGlobalSettings::cloudAccountName() const
{
    return m_cloudAccountNameAdaptor->value();
}

void QnGlobalSettings::setCloudAccountName(const QString& value)
{
    m_cloudAccountNameAdaptor->setValue(value);
}

const QString QnGlobalSettings::kNameCloudSystemID(lit("cloudSystemID"));

QString QnGlobalSettings::cloudSystemID() const
{
    return m_cloudSystemIDAdaptor->value();
}

void QnGlobalSettings::setCloudSystemID(const QString& value)
{
    m_cloudSystemIDAdaptor->setValue(value);
}

const QString QnGlobalSettings::kNameCloudAuthKey(lit("cloudAuthKey"));

QString QnGlobalSettings::cloudAuthKey() const
{
    return m_cloudAuthKeyAdaptor->value();
}

void QnGlobalSettings::setCloudAuthKey(const QString& value)
{
    m_cloudAuthKeyAdaptor->setValue(value);
}

void QnGlobalSettings::resetCloudParams()
{
    setCloudAccountName(QString());
    setCloudSystemID(QString());
    setCloudAuthKey(QString());
}

bool QnGlobalSettings::isNewSystem() const
{
    return m_newSystemAdaptor->value();
}

void QnGlobalSettings::setNewSystem(bool value)
{
    m_newSystemAdaptor->setValue(value);
}

bool QnGlobalSettings::arecontRtspEnabled() const
{
    return m_arecontRtspEnabledAdaptor->value();
}

void QnGlobalSettings::setArecontRtspEnabled(bool newVal) const
{
    m_arecontRtspEnabledAdaptor->setValue(newVal);
}

std::chrono::seconds QnGlobalSettings::proxyConnectTimeout() const
{
    return std::chrono::seconds(m_proxyConnectTimeoutAdaptor->value());
}

const QList<QnAbstractResourcePropertyAdaptor*>& QnGlobalSettings::allSettings() const
{
    return m_allAdaptors;
}
