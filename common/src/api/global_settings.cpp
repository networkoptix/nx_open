#include "global_settings.h"

#include <cassert>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>

#include "resource_property_adaptor.h"

#include <utils/common/app_info.h>
#include <utils/email/email.h>
#include <utils/common/ldap.h>
#include <utils/crypt/symmetrical.h>

#include <nx_ec/data/api_resource_data.h>
#include <common/common_module.h>

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


    const int kEc2ConnectionKeepAliveTimeoutDefault = 5;
    const int kEc2KeepAliveProbeCountDefault = 3;
    const QString kEc2AliveUpdateInterval(lit("ec2AliveUpdateIntervalSec"));
    const int kEc2AliveUpdateIntervalDefault = 60;
    const QString kServerDiscoveryPingTimeout(lit("serverDiscoveryPingTimeoutSec"));
    const int kServerDiscoveryPingTimeoutDefault = 60;

    const QString kArecontRtspEnabled(lit("arecontRtspEnabled"));
    const bool kArecontRtspEnabledDefault = false;
    const QString kSequentialFlirOnvifSearcherEnabled(lit("sequentialFlirOnvifSearcherEnabled"));
    const bool kSequentialFlirOnvifSearcherEnabledDefault = false;
    const QString kProxyConnectTimeout(lit("proxyConnectTimeoutSec"));
    const int kProxyConnectTimeoutDefault = 5;

    const QString kMaxRecorderQueueSizeBytesName(lit("maxRecordQueueSizeBytes"));
    const int kMaxRecorderQueueSizeBytesDefault = 1024 * 1024 * 24;
    const QString kMaxRecorderQueueSizePacketsName(lit("maxRecordQueueSizeElements"));
    const int kMaxRecorderQueueSizePacketsDefault = 1000;

    const QString kTakeCameraOwnershipWithoutLock(lit("takeCameraOwnershipWithoutLock"));
    const int kTakeCameraOwnershipWithoutLockDefault = false;

    const QString kMaxRtpRetryCount(lit("maxRtpRetryCount"));
    const int kMaxRtpRetryCountDefault(6);

    const int kAuditTrailPeriodDaysDefault = 183;
    const int kEventLogPeriodDaysDefault = 30;

    const QString kRtpTimeoutMs(lit("rtpTimeoutMs"));
    const int kRtpTimeoutMsDefault(10000);

    const QString kCloudConnectUdpHolePunchingEnabled(lit("cloudConnectUdpHolePunchingEnabled"));
    const bool kCloudConnectUdpHolePunchingEnabledDefault = true;

    const QString kCloudConnectRelayingEnabled(lit("cloudConnectRelayingEnabled"));
    const bool kCloudConnectRelayingEnabledDefault = true;

    const std::chrono::seconds kMaxDifferenceBetweenSynchronizedAndInternetDefault(20);
    const std::chrono::seconds kMaxDifferenceBetweenSynchronizedAndLocalTimeDefault(1);

    const QString kEnableEdgeRecording(lit("enableEdgeRecording"));
    const bool kEnableEdgeRecordingDefault(true);

    const QString kMaxRemoteArchiveSynchronizationThreads(
        lit("maxRemoteArchiveSynchronizationThreads"));
    const int kMaxRemoteArchiveSynchronizationThreadsDefault(-1);
}

using namespace nx::settings_names;

QnGlobalSettings::QnGlobalSettings(QObject *parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    NX_ASSERT(commonModule()->resourcePool());

    m_allAdaptors
        << initEmailAdaptors()
        << initLdapAdaptors()
        << initStaticticsAdaptors()
        << initConnectionAdaptors()
        << initTimeSynchronizationAdaptors()
        << initCloudAdaptors()
        << initMiscAdaptors()
        ;

    connect(commonModule()->resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (resource->getId() == QnUserResource::kAdminGuid)
                at_adminUserAdded(resource);
        }, Qt::DirectConnection);

    connect(commonModule()->resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnGlobalSettings::at_resourcePool_resourceRemoved, Qt::DirectConnection);
    initialize();
}

QnGlobalSettings::~QnGlobalSettings()
{
}

void QnGlobalSettings::initialize()
{
    if (isInitialized())
        return;
    const QnResourcePtr &resource = commonModule()->resourcePool()->getResourceById(QnUserResource::kAdminGuid);
    if (resource)
        at_adminUserAdded(resource);
}

bool QnGlobalSettings::isInitialized() const
{
    return !m_admin.isNull();
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initEmailAdaptors() {
    QString defaultSupportLink = QnAppInfo::supportUrl();
    if (defaultSupportLink.isEmpty())
        defaultSupportLink = QnAppInfo::supportEmailAddress();
    if (defaultSupportLink.isEmpty())
        defaultSupportLink = QnAppInfo::supportPhone();

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
    m_statisticsReportLastVersionAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameStatisticsReportLastVersion, QString(), this);
    m_statisticsReportLastNumberAdaptor = new QnLexicalResourcePropertyAdaptor<int>(kNameStatisticsReportLastNumber, 0, this);
    m_statisticsReportTimeCycleAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameStatisticsReportTimeCycle, QString(), this);
    m_statisticsReportUpdateDelayAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameStatisticsReportUpdateDelay, QString(), this);
    m_statisticsReportServerApiAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameStatisticsReportServerApi, QString(), this);
    m_clientStatisticsSettingsUrlAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameSettingsUrlParam, QString(), this);

    connect(m_statisticsAllowedAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this, &QnGlobalSettings::statisticsAllowedChanged, Qt::QueuedConnection);

    QnGlobalSettings::AdaptorList result;
    result
        << m_statisticsAllowedAdaptor
        << m_statisticsReportLastTimeAdaptor
        << m_statisticsReportLastVersionAdaptor
        << m_statisticsReportLastNumberAdaptor
        << m_statisticsReportTimeCycleAdaptor
        << m_statisticsReportUpdateDelayAdaptor
        << m_statisticsReportServerApiAdaptor
        << m_clientStatisticsSettingsUrlAdaptor
        ;

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initConnectionAdaptors()
{
    AdaptorList ec2Adaptors;
    m_ec2ConnectionKeepAliveTimeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kConnectionKeepAliveTimeoutKey,
        kEc2ConnectionKeepAliveTimeoutDefault,
        this);
    ec2Adaptors << m_ec2ConnectionKeepAliveTimeoutAdaptor;
    m_ec2KeepAliveProbeCountAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kKeepAliveProbeCountKey,
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
    m_proxyConnectTimeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kProxyConnectTimeout,
        kProxyConnectTimeoutDefault,
        this);
    ec2Adaptors << m_proxyConnectTimeoutAdaptor;
    m_takeCameraOwnershipWithoutLock = new QnLexicalResourcePropertyAdaptor<bool>(
        kTakeCameraOwnershipWithoutLock,
        kTakeCameraOwnershipWithoutLockDefault,
        this);
    ec2Adaptors << m_takeCameraOwnershipWithoutLock;

    for (auto adaptor : ec2Adaptors)
    {
        connect(adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this,
            [this, key = adaptor->key()]
            {
                emit ec2ConnectionSettingsChanged(key);
            }, Qt::QueuedConnection);
    }

    return ec2Adaptors;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initTimeSynchronizationAdaptors()
{
    using namespace std::chrono;

    QList<QnAbstractResourcePropertyAdaptor*> timeSynchronizationAdaptors;
    m_timeSynchronizationEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameTimeSynchronizationEnabled,
        true,
        this);
    timeSynchronizationAdaptors << m_timeSynchronizationEnabledAdaptor;

    m_synchronizeTimeWithInternetAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameSynchronizeTimeWithInternet,
        true,
        this);
    timeSynchronizationAdaptors << m_synchronizeTimeWithInternetAdaptor;

    m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            kMaxDifferenceBetweenSynchronizedAndInternetTime,
            duration_cast<milliseconds>(kMaxDifferenceBetweenSynchronizedAndInternetDefault).count(),
            this);
    timeSynchronizationAdaptors << m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor;

    m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            kMaxDifferenceBetweenSynchronizedAndLocalTime,
            duration_cast<milliseconds>(kMaxDifferenceBetweenSynchronizedAndLocalTimeDefault).count(),
            this);
    timeSynchronizationAdaptors << m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor;

    for (auto adaptor: timeSynchronizationAdaptors)
    {
        connect(
            adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
            this, &QnGlobalSettings::timeSynchronizationSettingsChanged,
            Qt::QueuedConnection);
    }

    return timeSynchronizationAdaptors;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initCloudAdaptors()
{
    m_cloudAccountNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameCloudAccountName, QString(), this);
    m_cloudSystemIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameCloudSystemId, QString(), this);
    m_cloudAuthKeyAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameCloudAuthKey, QString(), this);

    QnGlobalSettings::AdaptorList result;
    result
        << m_cloudAccountNameAdaptor
        << m_cloudSystemIdAdaptor
        << m_cloudAuthKeyAdaptor
        ;

    for (QnAbstractResourcePropertyAdaptor* adaptor : result)
        connect(adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this, &QnGlobalSettings::cloudSettingsChanged, Qt::QueuedConnection);

    connect(
        m_cloudSystemIdAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
        this, &QnGlobalSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);
    connect(
        m_cloudAuthKeyAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
        this, &QnGlobalSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initMiscAdaptors()
{
    m_systemNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameSystemName, QString(), this);
    m_localSystemIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameLocalSystemId, QString(), this);
    m_disabledVendorsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameDisabledVendors, QString(), this);
    m_cameraSettingsOptimizationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameCameraSettingsOptimization, true, this);
    m_autoUpdateThumbnailsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameAutoUpdateThumbnails, true, this);
    m_maxSceneItemsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(kMaxSceneItemsOverrideKey, 0, this);
    m_useTextEmailFormatAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kUseTextEmailFormat, false, this);
    m_auditTrailEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameAuditTrailEnabled, true, this);
    m_auditTrailPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kAuditTrailPeriodDaysName,
        kAuditTrailPeriodDaysDefault,
        this);
    m_eventLogPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kEventLogPeriodDaysName,
        kEventLogPeriodDaysDefault,
        this);

    m_autoDiscoveryEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameAutoDiscoveryEnabled, true, this);
    m_updateNotificationsEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameUpdateNotificationsEnabled, true, this);
    m_backupQualitiesAdaptor = new QnLexicalResourcePropertyAdaptor<Qn::CameraBackupQualities>(kNameBackupQualities, Qn::CameraBackup_Both, this);
    m_backupNewCamerasByDefaultAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameBackupNewCamerasByDefault, false, this);
    m_upnpPortMappingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameUpnpPortMappingEnabled, true, this);
    m_cloudHostAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kCloudHostName, QString(), this);

    m_arecontRtspEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kArecontRtspEnabled,
        kArecontRtspEnabledDefault,
        this);

    m_sequentialFlirOnvifSearcherEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kSequentialFlirOnvifSearcherEnabled,
        kSequentialFlirOnvifSearcherEnabledDefault,
        this);

    m_maxRecorderQueueSizeBytes = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRecorderQueueSizeBytesName,
        kMaxRecorderQueueSizeBytesDefault,
        this);

    m_maxRecorderQueueSizePackets = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRecorderQueueSizePacketsName,
        kMaxRecorderQueueSizePacketsDefault,
        this);

    m_maxRtpRetryCount = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRtpRetryCount,
        kMaxRtpRetryCountDefault,
        this);

    m_rtpFrameTimeoutMs = new QnLexicalResourcePropertyAdaptor<int>(
        kRtpTimeoutMs,
        kRtpTimeoutMsDefault,
        this);

    m_cloudConnectUdpHolePunchingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kCloudConnectUdpHolePunchingEnabled,
        kCloudConnectUdpHolePunchingEnabledDefault,
        this);

    m_cloudConnectRelayingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kCloudConnectRelayingEnabled,
        kCloudConnectRelayingEnabledDefault,
        this);

    m_edgeRecordingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kEnableEdgeRecording,
        kEnableEdgeRecordingDefault,
        this);

    m_maxRemoteArchiveSynchronizationThreads = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRemoteArchiveSynchronizationThreads,
        kMaxRemoteArchiveSynchronizationThreadsDefault,
        this);

    connect(m_systemNameAdaptor,                    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::systemNameChanged,                   Qt::QueuedConnection);
    connect(m_localSystemIdAdaptor,                 &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::localSystemIdChanged,                Qt::QueuedConnection);
    connect(m_disabledVendorsAdaptor,               &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::disabledVendorsChanged,              Qt::QueuedConnection);
    connect(m_auditTrailEnabledAdaptor,             &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::auditTrailEnableChanged,             Qt::QueuedConnection);
    connect(m_auditTrailPeriodDaysAdaptor,          &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::auditTrailPeriodDaysChanged,         Qt::QueuedConnection);
    connect(m_eventLogPeriodDaysAdaptor,            &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::eventLogPeriodDaysChanged,           Qt::QueuedConnection);
    connect(m_cameraSettingsOptimizationAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::cameraSettingsOptimizationChanged,   Qt::QueuedConnection);
    connect(m_autoUpdateThumbnailsAdaptor,          &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::autoUpdateThumbnailsChanged,         Qt::QueuedConnection);
    connect(m_maxSceneItemsAdaptor,                 &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::maxSceneItemsChanged, Qt::DirectConnection); //< I need this one now :)
    connect(m_useTextEmailFormatAdaptor,            &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::useTextEmailFormatChanged,           Qt::QueuedConnection);
    connect(m_autoDiscoveryEnabledAdaptor,          &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::autoDiscoveryChanged,                Qt::QueuedConnection);
    connect(m_updateNotificationsEnabledAdaptor,    &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::updateNotificationsChanged,          Qt::QueuedConnection);
    connect(m_upnpPortMappingEnabledAdaptor,        &QnAbstractResourcePropertyAdaptor::valueChanged,   this,   &QnGlobalSettings::upnpPortMappingEnabledChanged,       Qt::QueuedConnection);
    connect(
        m_cloudConnectUdpHolePunchingEnabledAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
        this, &QnGlobalSettings::cloudConnectUdpHolePunchingEnabledChanged,
        Qt::QueuedConnection);
    connect(
        m_cloudConnectRelayingEnabledAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
        this, &QnGlobalSettings::cloudConnectRelayingEnabledChanged,
        Qt::QueuedConnection);

    QnGlobalSettings::AdaptorList result;
    result
        << m_systemNameAdaptor
        << m_localSystemIdAdaptor
        << m_disabledVendorsAdaptor
        << m_cameraSettingsOptimizationAdaptor
        << m_autoUpdateThumbnailsAdaptor
        << m_maxSceneItemsAdaptor
        << m_useTextEmailFormatAdaptor
        << m_auditTrailEnabledAdaptor
        << m_auditTrailPeriodDaysAdaptor
        << m_eventLogPeriodDaysAdaptor
        << m_autoDiscoveryEnabledAdaptor
        << m_updateNotificationsEnabledAdaptor
        << m_backupQualitiesAdaptor
        << m_backupNewCamerasByDefaultAdaptor
        << m_upnpPortMappingEnabledAdaptor
        << m_cloudHostAdaptor
        << m_arecontRtspEnabledAdaptor
        << m_sequentialFlirOnvifSearcherEnabledAdaptor
        << m_maxRecorderQueueSizeBytes
        << m_maxRecorderQueueSizePackets
        << m_rtpFrameTimeoutMs
        << m_cloudConnectUdpHolePunchingEnabledAdaptor
        << m_cloudConnectRelayingEnabledAdaptor
        << m_edgeRecordingEnabledAdaptor
        << m_maxRemoteArchiveSynchronizationThreads
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

bool QnGlobalSettings::isAutoUpdateThumbnailsEnabled() const
{
    return m_autoUpdateThumbnailsAdaptor->value();
}

void QnGlobalSettings::setAutoUpdateThumbnailsEnabled(bool value)
{
    m_autoUpdateThumbnailsAdaptor->setValue(value);
}

int QnGlobalSettings::maxSceneItemsOverride() const
{
    return m_maxSceneItemsAdaptor->value();
}

void QnGlobalSettings::setMaxSceneItemsOverride(int value)
{
    m_maxSceneItemsAdaptor->setValue(value);
}

bool QnGlobalSettings::isUseTextEmailFormat() const
{
    return m_useTextEmailFormatAdaptor->value();
}

void QnGlobalSettings::setUseTextEmailFormat(bool value)
{
    m_useTextEmailFormatAdaptor->setValue(value);
}

bool QnGlobalSettings::isAuditTrailEnabled() const
{
    return m_auditTrailEnabledAdaptor->value();
}

void QnGlobalSettings::setAuditTrailEnabled(bool value)
{
    m_auditTrailEnabledAdaptor->setValue(value);
}

int QnGlobalSettings::auditTrailPeriodDays() const
{
    return m_auditTrailPeriodDaysAdaptor->value();
}

int QnGlobalSettings::eventLogPeriodDays() const
{
    return m_eventLogPeriodDaysAdaptor->value();
}

bool QnGlobalSettings::isAutoDiscoveryEnabled() const {
    return m_autoDiscoveryEnabledAdaptor->value();
}

void QnGlobalSettings::setAutoDiscoveryEnabled(bool enabled)
{
    m_autoDiscoveryEnabledAdaptor->setValue(enabled);
}

void QnGlobalSettings::at_adminUserAdded(const QnResourcePtr &resource)
{
    if(m_admin)
        return;

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    // todo: refactor this. Globabal settings should be moved from admin user to another place
    if(!user->isBuiltInAdmin())
        return;

    {
        QnMutexLocker locker(&m_mutex);

        m_admin = user;
        for (QnAbstractResourcePropertyAdaptor* adaptor : m_allAdaptors)
            adaptor->setResource(user);
    }
    emit initialized();
}

void QnGlobalSettings::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
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
    result.adminPassword = nx::utils::decodeStringFromHexStringAES128CBC(m_ldapAdminPasswordAdaptor->value());
    result.searchBase = m_ldapSearchBaseAdaptor->value();
    result.searchFilter = m_ldapSearchFilterAdaptor->value();
    return result;
}

void QnGlobalSettings::setLdapSettings(const QnLdapSettings &settings)
{
    m_ldapUriAdaptor->setValue(settings.uri);
    m_ldapAdminDnAdaptor->setValue(settings.adminDn);
    m_ldapAdminPasswordAdaptor->setValue(
        settings.isValid()
        ? nx::utils::encodeHexStringFromStringAES128CBC(settings.adminPassword)
        : QString());
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
    result.password = nx::utils::decodeStringFromHexStringAES128CBC(m_passwordAdaptor->value());
    result.connectionType = m_connectionTypeAdaptor->value();
    result.signature = m_signatureAdaptor->value();
    result.supportEmail = m_supportLinkAdaptor->value();
    result.simple = m_simpleAdaptor->value();
    result.timeout = m_timeoutAdaptor->value();

    /*
     * VMS-1055 - default email changed to link.
     * We are checking if the value is not overridden and replacing it by the updated one.
     */
    if (result.supportEmail == QnAppInfo::supportEmailAddress() &&
        !QnAppInfo::supportUrl().isEmpty())
    {
        result.supportEmail = QnAppInfo::supportUrl();
    }

    return result;
}

void QnGlobalSettings::setEmailSettings(const QnEmailSettings &settings)
{
    m_serverAdaptor->setValue(settings.server);
    m_fromAdaptor->setValue(settings.email);
    m_portAdaptor->setValue(settings.port == QnEmailSettings::defaultPort(settings.connectionType) ? 0 : settings.port);
    m_userAdaptor->setValue(settings.user);
    m_passwordAdaptor->setValue(
        settings.isValid()
        ? nx::utils::encodeHexStringFromStringAES128CBC(settings.password)
        : QString());
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
    //NX_ASSERT(m_admin, Q_FUNC_INFO, "Invalid sync state");
    if (!m_admin)
        return;
    propertyDictionary()->saveParamsAsync(m_admin->getId());
}

bool QnGlobalSettings::resynchronizeNowSync()
{
    {
        QnMutexLocker locker(&m_mutex);
        NX_ASSERT(m_admin, Q_FUNC_INFO, "Invalid sync state");
        if (!m_admin)
            return false;
        propertyDictionary()->markAllParamsDirty(m_admin->getId());
    }
    return  synchronizeNowSync();
}

bool QnGlobalSettings::synchronizeNowSync()
{
    for (QnAbstractResourcePropertyAdaptor* adaptor : m_allAdaptors)
        adaptor->saveToResource();

    QnMutexLocker locker(&m_mutex);
    NX_ASSERT(m_admin, Q_FUNC_INFO, "Invalid sync state");
    if (!m_admin)
        return false;
    return propertyDictionary()->saveParams(m_admin->getId());
}

bool QnGlobalSettings::takeFromSettings(QSettings* settings, const QnResourcePtr& mediaServer)
{
    bool changed = false;

    changed |= m_statisticsReportTimeCycleAdaptor->takeFromSettings(settings);
    changed |= m_statisticsReportUpdateDelayAdaptor->takeFromSettings(settings);
    changed |= m_statisticsReportServerApiAdaptor->takeFromSettings(settings);
    changed |= m_clientStatisticsSettingsUrlAdaptor->takeFromSettings(settings);

    changed |= m_cloudSystemIdAdaptor->takeFromSettings(settings);
    changed |= m_cloudAuthKeyAdaptor->takeFromSettings(settings);

    /**
     * Fix statistics allowed flag by value, set in the installer.
     * Note that installer value will override the existing one.
     */
    if (m_statisticsAllowedAdaptor->takeFromSettings(settings))
    {
        changed = true;
    }
    else
    {
        static const QString kStatisticsReportAllowed = lit("statisticsReportAllowed");
        /* If user didn't make the decision in the current version, check if he made it in the previous version */
        if (!isStatisticsAllowedDefined() && mediaServer && mediaServer->hasProperty(kStatisticsReportAllowed))
        {
            bool value;
            if (QnLexical::deserialize(mediaServer->getProperty(kStatisticsReportAllowed), &value))
            {
                changed = true;
                m_statisticsAllowedAdaptor->setValue(QnOptionalBool(value));
            }
            propertyDictionary()->removeProperty(mediaServer->getId(), kStatisticsReportAllowed);
        }
    }


    return changed ? synchronizeNowSync() : false;
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

QString QnGlobalSettings::statisticsReportLastVersion() const
{
    return m_statisticsReportLastVersionAdaptor->value();
}

void QnGlobalSettings::setStatisticsReportLastVersion(const QString& value)
{
    m_statisticsReportLastVersionAdaptor->setValue(value);
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

QString QnGlobalSettings::statisticsReportUpdateDelay() const
{
    return m_statisticsReportUpdateDelayAdaptor->value();
}

void QnGlobalSettings::setStatisticsReportUpdateDelay(const QString& value)
{
    m_statisticsReportUpdateDelayAdaptor->setValue(value);
}

bool QnGlobalSettings::isUpnpPortMappingEnabled() const
{
    return m_upnpPortMappingEnabledAdaptor->value();
}

void QnGlobalSettings::setUpnpPortMappingEnabled(bool value)
{
    m_upnpPortMappingEnabledAdaptor->setValue(value);
}

QnUuid QnGlobalSettings::localSystemId() const
{
    return QnUuid(m_localSystemIdAdaptor->value());
}

void QnGlobalSettings::setLocalSystemId(const QnUuid& value)
{
    m_localSystemIdAdaptor->setValue(value.toString());
}

QString QnGlobalSettings::clientStatisticsSettingsUrl() const
{
    return m_clientStatisticsSettingsUrlAdaptor->value();
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

void QnGlobalSettings::setTimeSynchronizationEnabled(bool value)
{
    m_timeSynchronizationEnabledAdaptor->setValue(value);
}

bool QnGlobalSettings::isSynchronizingTimeWithInternet() const
{
    return m_synchronizeTimeWithInternetAdaptor->value();
}

void QnGlobalSettings::setSynchronizingTimeWithInternet(bool value)
{
    m_synchronizeTimeWithInternetAdaptor->setValue(value);
}

std::chrono::milliseconds QnGlobalSettings::maxDifferenceBetweenSynchronizedAndInternetTime() const
{
    return std::chrono::milliseconds(
        m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor->value());
}

std::chrono::milliseconds QnGlobalSettings::maxDifferenceBetweenSynchronizedAndLocalTime() const
{
    return std::chrono::milliseconds(
        m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor->value());
}

QString QnGlobalSettings::cloudAccountName() const
{
    return m_cloudAccountNameAdaptor->value();
}

void QnGlobalSettings::setCloudAccountName(const QString& value)
{
    m_cloudAccountNameAdaptor->setValue(value);
}

QString QnGlobalSettings::cloudSystemId() const
{
    return m_cloudSystemIdAdaptor->value();
}

void QnGlobalSettings::setCloudSystemId(const QString& value)
{
    m_cloudSystemIdAdaptor->setValue(value);
}

QString QnGlobalSettings::cloudAuthKey() const
{
    return nx::utils::decodeStringFromHexStringAES128CBC(m_cloudAuthKeyAdaptor->value());
}

void QnGlobalSettings::setCloudAuthKey(const QString& value)
{
    m_cloudAuthKeyAdaptor->setValue(nx::utils::encodeHexStringFromStringAES128CBC(value));
}

QString QnGlobalSettings::systemName() const
{
    return m_systemNameAdaptor->value();
}

void QnGlobalSettings::setSystemName(const QString& value)
{
    m_systemNameAdaptor->setValue(value);
}

void QnGlobalSettings::resetCloudParams()
{
    setCloudAccountName(QString());
    setCloudSystemId(QString());
    setCloudAuthKey(QString());
}

QString QnGlobalSettings::cloudHost() const
{
    return m_cloudHostAdaptor->value();
}

void QnGlobalSettings::setCloudHost(const QString& value)
{
    m_cloudHostAdaptor->setValue(value);
}

bool QnGlobalSettings::arecontRtspEnabled() const
{
    return m_arecontRtspEnabledAdaptor->value();
}

void QnGlobalSettings::setArecontRtspEnabled(bool newVal) const
{
    m_arecontRtspEnabledAdaptor->setValue(newVal);
}

int QnGlobalSettings::maxRtpRetryCount() const
{
    return m_maxRtpRetryCount->value();
}

void QnGlobalSettings::setMaxRtpRetryCount(int newVal)
{
    m_maxRtpRetryCount->setValue(newVal);
}

bool QnGlobalSettings::sequentialFlirOnvifSearcherEnabled() const
{
    return m_sequentialFlirOnvifSearcherEnabledAdaptor->value();
}

void QnGlobalSettings::setSequentialFlirOnvifSearcherEnabled(bool newVal)
{
    m_sequentialFlirOnvifSearcherEnabledAdaptor->setValue(newVal);
}

int QnGlobalSettings::rtpFrameTimeoutMs() const
{
    return m_rtpFrameTimeoutMs->value();
}

void QnGlobalSettings::setRtpFrameTimeoutMs(int newValue)
{
    m_rtpFrameTimeoutMs->setValue(newValue);
}

int QnGlobalSettings::maxRecorderQueueSizeBytes() const
{
    return m_maxRecorderQueueSizeBytes->value();
}

int QnGlobalSettings::maxRecorderQueueSizePackets() const
{
    return m_maxRecorderQueueSizePackets->value();
}

bool QnGlobalSettings::isEdgeRecordingEnabled() const
{
    return m_edgeRecordingEnabledAdaptor->value();
}

void QnGlobalSettings::setEdgeRecordingEnabled(bool enabled)
{
    m_edgeRecordingEnabledAdaptor->setValue(enabled);
}

int QnGlobalSettings::maxRemoteArchiveSynchronizationThreads() const
{
    return m_maxRemoteArchiveSynchronizationThreads->value();
}

void QnGlobalSettings::setMaxRemoteArchiveSynchronizationThreads(int newValue)
{
    m_maxRemoteArchiveSynchronizationThreads->setValue(newValue);
}

std::chrono::seconds QnGlobalSettings::proxyConnectTimeout() const
{
    return std::chrono::seconds(m_proxyConnectTimeoutAdaptor->value());
}

bool QnGlobalSettings::cloudConnectUdpHolePunchingEnabled() const
{
    return m_cloudConnectUdpHolePunchingEnabledAdaptor->value();
}

bool QnGlobalSettings::cloudConnectRelayingEnabled() const
{
    return m_cloudConnectRelayingEnabledAdaptor->value();
}

bool QnGlobalSettings::takeCameraOwnershipWithoutLock() const
{
    return m_takeCameraOwnershipWithoutLock->value();
}

const QList<QnAbstractResourcePropertyAdaptor*>& QnGlobalSettings::allSettings() const
{
    return m_allAdaptors;
}

bool QnGlobalSettings::isGlobalSetting(const ec2::ApiResourceParamWithRefData& param)
{
    return QnUserResource::kAdminGuid == param.resourceId;
}
