#include "global_settings.h"

#include <cassert>

#include <nx/utils/log/log.h>

#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>

#include <api/resource_property_adaptor.h>

#include <utils/common/app_info.h>
#include <utils/email/email.h>
#include <utils/common/ldap.h>
#include <utils/common/watermark_settings.h>
#include <utils/crypt/symmetrical.h>

#include <nx/utils/app_info.h>
#include <nx/vms/api/data/resource_data.h>

namespace {

QSet<QString> parseDisabledVendors(const QString& disabledVendors)
{
    QStringList disabledVendorList;
    if (disabledVendors.contains(";"))
        disabledVendorList = disabledVendors.split(";");
    else
        disabledVendorList = disabledVendors.split(" ");

    QStringList updatedVendorList;
    for (int i = 0; i < disabledVendorList.size(); ++i)
    {
        if (!disabledVendorList[i].trimmed().isEmpty())
        {
            updatedVendorList << disabledVendorList[i].trimmed();
        }
    }

    return updatedVendorList.toSet();
}

const int kEc2ConnectionKeepAliveTimeoutDefault = 5;
const int kEc2KeepAliveProbeCountDefault = 3;
const QString kEc2AliveUpdateInterval("ec2AliveUpdateIntervalSec");
const int kEc2AliveUpdateIntervalDefault = 60;
const QString kServerDiscoveryPingTimeout("serverDiscoveryPingTimeoutSec");
const int kServerDiscoveryPingTimeoutDefault = 60;

const QString kArecontRtspEnabled("arecontRtspEnabled");
const bool kArecontRtspEnabledDefault = false;
const QString kSequentialFlirOnvifSearcherEnabled("sequentialFlirOnvifSearcherEnabled");
const bool kSequentialFlirOnvifSearcherEnabledDefault = false;
const QString kProxyConnectTimeout("proxyConnectTimeoutSec");
const int kProxyConnectTimeoutDefault = 5;

const QString kMaxP2pQueueSizeBytesName("maxP2pQueueSizeBytes");
const QString kMaxP2pQueueSizeForAllClientsBytesName("maxP2pAllClientsSizeBytes");
const int kMaxP2pQueueSizeBytesDefault = 1024 * 1024 * 50;
const qint64 kMaxP2pQueueSizeForAllClientsBytesDefault = 1024 * 1024 * 1024;
const QString kMaxRecorderQueueSizeBytesName("maxRecordQueueSizeBytes");
const int kMaxRecorderQueueSizeBytesDefault = 1024 * 1024 * 24;
const QString kMaxRecorderQueueSizePacketsName("maxRecordQueueSizeElements");
const int kMaxRecorderQueueSizePacketsDefault = 1000;

const QString kMaxWebMTranscodersName("maxWebMTranscoders");
const int kMaxWebMTranscodersDefault = 2;

const QString kTakeCameraOwnershipWithoutLock("takeCameraOwnershipWithoutLock");
const int kTakeCameraOwnershipWithoutLockDefault = true;

const QString kMaxRtpRetryCount("maxRtpRetryCount");
const int kMaxRtpRetryCountDefault(6);

const int kAuditTrailPeriodDaysDefault = 183;
const int kEventLogPeriodDaysDefault = 30;

const QString kRtpTimeoutMs("rtpTimeoutMs");
const int kRtpTimeoutMsDefault(10000);

const QString kRtspClientMaxSessionDurationKey("maxRtspConnectDurationSeconds");
const std::chrono::seconds kRtspClientMaxSessionDurationDefault(0);

const QString kCloudConnectUdpHolePunchingEnabled("cloudConnectUdpHolePunchingEnabled");
const bool kCloudConnectUdpHolePunchingEnabledDefault = true;

const QString kCloudConnectRelayingEnabled("cloudConnectRelayingEnabled");
const bool kCloudConnectRelayingEnabledDefault = true;

const std::chrono::seconds kMaxDifferenceBetweenSynchronizedAndInternetDefault(2);
const std::chrono::seconds kMaxDifferenceBetweenSynchronizedAndLocalTimeDefault(5);
const std::chrono::seconds kOsTimeChangeCheckPeriodDefault(1);
const std::chrono::minutes kSyncTimeExchangePeriodDefault(10);
const std::chrono::milliseconds kSyncTimeEpsilonDefault(200);

const QString kEnableEdgeRecording(lit("enableEdgeRecording"));
const QString kEnableWebSocketKey(lit("webSocketEnabled"));
const bool kEnableEdgeRecordingDefault(true);

const QString kMaxRemoteArchiveSynchronizationThreads(
    "maxRemoteArchiveSynchronizationThreads");
const int kMaxRemoteArchiveSynchronizationThreadsDefault(-1);

const QString kMaxWearableArchiveSynchronizationThreads(
    "maxWearableArchiveSynchronizationThreads");
const int kMaxWearableArchiveSynchronizationThreadsDefault(-1);

} // namespace

using namespace nx::settings_names;

QnGlobalSettings::QnGlobalSettings(QObject* parent):
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

    const auto& resource = commonModule()->resourcePool()->getResourceById(
        QnUserResource::kAdminGuid);

    if (resource)
        at_adminUserAdded(resource);
}

bool QnGlobalSettings::isInitialized() const
{
    return !m_admin.isNull();
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initEmailAdaptors()
{
    m_serverAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameHost, QString(), this);
    m_fromAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameFrom, QString(), this);
    m_userAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(kNameUser, QString(), this);
    m_passwordAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNamePassword,
        QString(),
        this);
    m_signatureAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameSignature,
        QString(),
        this);
    m_supportLinkAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameSupportEmail,
        nx::utils::AppInfo::supportAddress(),
        this);
    m_connectionTypeAdaptor = new QnLexicalResourcePropertyAdaptor<QnEmail::ConnectionType>(
        kNameConnectionType,
        QnEmail::ConnectionType::unsecure,
        this);
    m_portAdaptor = new QnLexicalResourcePropertyAdaptor<int>(kNamePort, 0, this);
    m_timeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(kNameTimeout,
        QnEmailSettings::defaultTimeoutSec(),
        this);
    m_simpleAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(kNameSimple, true, this);

    AdaptorList result;
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

    for (auto adaptor: result)
    {
        connect(
            adaptor,
            &QnAbstractResourcePropertyAdaptor::valueChanged,
            this,
            &QnGlobalSettings::emailSettingsChanged,
            Qt::QueuedConnection);
    }

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initLdapAdaptors()
{
    m_ldapUriAdaptor = new QnLexicalResourcePropertyAdaptor<QUrl>(ldapUri, QUrl(), this);
    m_ldapAdminDnAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        ldapAdminDn,
        QString(),
        this);
    m_ldapAdminPasswordAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        ldapAdminPassword,
        QString(),
        this);
    m_ldapSearchBaseAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        ldapSearchBase,
        QString(),
        this);
    m_ldapSearchFilterAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        ldapSearchFilter,
        QString(),
        this);
    m_ldapSearchTimeoutSAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        ldapSearchTimeoutS, ldapSearchTimeoutSDefault, this);

    AdaptorList result;
    result
        << m_ldapUriAdaptor
        << m_ldapAdminDnAdaptor
        << m_ldapAdminPasswordAdaptor
        << m_ldapSearchBaseAdaptor
        << m_ldapSearchFilterAdaptor
        << m_ldapSearchTimeoutSAdaptor;
    for (auto adaptor: result)
    {
        connect(
            adaptor,
            &QnAbstractResourcePropertyAdaptor::valueChanged,
            this,
            &QnGlobalSettings::ldapSettingsChanged,
            Qt::QueuedConnection);
    }

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initStaticticsAdaptors()
{
    m_statisticsAllowedAdaptor = new QnLexicalResourcePropertyAdaptor<QnOptionalBool>(
        kNameStatisticsAllowed,
        QnOptionalBool(),
        this);
    m_statisticsReportLastTimeAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameStatisticsReportLastTime,
        QString(),
        this);
    m_statisticsReportLastVersionAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameStatisticsReportLastVersion,
        QString(),
        this);
    m_statisticsReportLastNumberAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kNameStatisticsReportLastNumber,
        0,
        this);
    m_statisticsReportTimeCycleAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameStatisticsReportTimeCycle,
        QString(),
        this);
    m_statisticsReportUpdateDelayAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameStatisticsReportUpdateDelay,
        QString(),
        this);
    m_statisticsReportServerApiAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameStatisticsReportServerApi,
        QString(),
        this);
    m_clientStatisticsSettingsUrlAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameSettingsUrlParam,
        QString(),
        this);

    connect(
        m_statisticsAllowedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::statisticsAllowedChanged,
        Qt::QueuedConnection);

    AdaptorList result;
    result
        << m_statisticsAllowedAdaptor
        << m_statisticsReportLastTimeAdaptor
        << m_statisticsReportLastVersionAdaptor
        << m_statisticsReportLastNumberAdaptor
        << m_statisticsReportTimeCycleAdaptor
        << m_statisticsReportUpdateDelayAdaptor
        << m_statisticsReportServerApiAdaptor
        << m_clientStatisticsSettingsUrlAdaptor;

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

    for (auto adaptor: ec2Adaptors)
    {
        connect(
            adaptor,
            &QnAbstractResourcePropertyAdaptor::valueChanged,
            this,
            [this, key = adaptor->key()]
            {
                emit ec2ConnectionSettingsChanged(key);
            },
            Qt::QueuedConnection);
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

    m_primaryTimeServerAdaptor = new QnLexicalResourcePropertyAdaptor<QnUuid>(
        kNamePrimaryTimeServer,
        QnUuid(),
        this);
    timeSynchronizationAdaptors << m_primaryTimeServerAdaptor;

    m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            kMaxDifferenceBetweenSynchronizedAndInternetTime,
            duration_cast<milliseconds>(
                kMaxDifferenceBetweenSynchronizedAndInternetDefault).count(),
            this);
    timeSynchronizationAdaptors << m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor;

    m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            kMaxDifferenceBetweenSynchronizedAndLocalTime,
            duration_cast<milliseconds>(
                kMaxDifferenceBetweenSynchronizedAndLocalTimeDefault).count(),
            this);
    timeSynchronizationAdaptors << m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor;

    m_osTimeChangeCheckPeriodAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            kOsTimeChangeCheckPeriod,
            duration_cast<milliseconds>(kOsTimeChangeCheckPeriodDefault).count(),
            this);
    timeSynchronizationAdaptors << m_osTimeChangeCheckPeriodAdaptor;

    m_syncTimeExchangePeriodAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            kSyncTimeExchangePeriod,
            duration_cast<milliseconds>(kSyncTimeExchangePeriodDefault).count(),
            this);
    timeSynchronizationAdaptors << m_syncTimeExchangePeriodAdaptor;

    m_syncTimeEpsilonAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            kSyncTimeEpsilon,
            duration_cast<milliseconds>(kSyncTimeEpsilonDefault).count(),
            this);
    timeSynchronizationAdaptors << m_syncTimeEpsilonAdaptor;

    for (auto adaptor: timeSynchronizationAdaptors)
    {
        connect(
            adaptor,
            &QnAbstractResourcePropertyAdaptor::valueChanged,
            this,
            &QnGlobalSettings::timeSynchronizationSettingsChanged,
            Qt::QueuedConnection);
    }

    return timeSynchronizationAdaptors;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initCloudAdaptors()
{
    m_cloudAccountNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameCloudAccountName,
        QString(),
        this);

    m_cloudSystemIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameCloudSystemId,
        QString(),
        this);

    m_cloudAuthKeyAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameCloudAuthKey,
        QString(),
        this);

    AdaptorList result;
    result
        << m_cloudAccountNameAdaptor
        << m_cloudSystemIdAdaptor
        << m_cloudAuthKeyAdaptor;

    for (auto adaptor: result)
    {
        connect(
            adaptor,
            &QnAbstractResourcePropertyAdaptor::valueChanged,
            this,
            &QnGlobalSettings::cloudSettingsChanged,
            Qt::QueuedConnection);
    }

    connect(
        m_cloudSystemIdAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudAuthKeyAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);

    return result;
}

QnGlobalSettings::AdaptorList QnGlobalSettings::initMiscAdaptors()
{
    using namespace std::chrono;

    m_systemNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameSystemName,
        QString(),
        this);

    m_localSystemIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameLocalSystemId,
        QString(),
        this);

    m_lastMergeMasterIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameLastMergeMasterId,
        QString(),
        this);
    m_lastMergeSlaveIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameLastMergeSlaveId,
        QString(),
        this);

    m_disabledVendorsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kNameDisabledVendors,
        QString(),
        this);

    m_cameraSettingsOptimizationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameCameraSettingsOptimization,
        true,
        this);

    m_autoUpdateThumbnailsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameAutoUpdateThumbnails,
        true,
        this);

    m_maxSceneItemsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxSceneItemsOverrideKey,
        0,
        this);

    m_useTextEmailFormatAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kUseTextEmailFormat,
        false,
        this);

    m_useWindowsEmailLineFeedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kUseWindowsEmailLineFeed,
        false,
        this);

    m_auditTrailEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameAuditTrailEnabled,
        true,
        this);

    m_auditTrailPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kAuditTrailPeriodDaysName,
        kAuditTrailPeriodDaysDefault,
        this);

    m_eventLogPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kEventLogPeriodDaysName,
        kEventLogPeriodDaysDefault,
        this);

    m_trafficEncryptionForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameTrafficEncryptionForced,
        false,
        this);

    m_videoTrafficEncryptionForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameVideoTrafficEncryptionForced,
        false,
        this);

    m_autoDiscoveryEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameAutoDiscoveryEnabled,
        true,
        this);

    m_autoDiscoveryResponseEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameAutoDiscoveryResponseEnabled,
        true,
        this);

    m_updateNotificationsEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameUpdateNotificationsEnabled,
        true,
        this);

    m_backupQualitiesAdaptor = new QnLexicalResourcePropertyAdaptor<Qn::CameraBackupQualities>(
        kNameBackupQualities,
        Qn::CameraBackupQuality::CameraBackup_Both,
        this);

    m_backupNewCamerasByDefaultAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameBackupNewCamerasByDefault,
        false,
        this);

    m_upnpPortMappingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kNameUpnpPortMappingEnabled,
        true,
        this);

    m_cloudHostAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kCloudHostName,
        QString(),
        this);

    m_arecontRtspEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kArecontRtspEnabled,
        kArecontRtspEnabledDefault,
        this);

    m_sequentialFlirOnvifSearcherEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kSequentialFlirOnvifSearcherEnabled,
        kSequentialFlirOnvifSearcherEnabledDefault,
        this);

    m_maxP2pQueueSizeBytes = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxP2pQueueSizeBytesName,
        kMaxP2pQueueSizeBytesDefault,
        this);

    m_maxP2pQueueSizeForAllClientsBytes = new QnLexicalResourcePropertyAdaptor<qint64>(
        kMaxP2pQueueSizeForAllClientsBytesName,
        kMaxP2pQueueSizeForAllClientsBytesDefault,
        this);

    m_maxRecorderQueueSizeBytes = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRecorderQueueSizeBytesName,
        kMaxRecorderQueueSizeBytesDefault,
        this);

    m_maxRecorderQueueSizePackets = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRecorderQueueSizePacketsName,
        kMaxRecorderQueueSizePacketsDefault,
        this);

    m_maxWebMTranscoders = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxWebMTranscodersName, kMaxWebMTranscodersDefault, this);

    m_maxRtpRetryCount = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRtpRetryCount,
        kMaxRtpRetryCountDefault,
        this);

    m_rtpFrameTimeoutMs = new QnLexicalResourcePropertyAdaptor<int>(
        kRtpTimeoutMs,
        kRtpTimeoutMsDefault,
        this);

    m_maxRtspConnectDuration = new QnLexicalResourcePropertyAdaptor<int>(
        kRtspClientMaxSessionDurationKey,
        kRtspClientMaxSessionDurationDefault.count(),
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

    m_webSocketEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        kEnableWebSocketKey,
        /*default value*/ true,
        this);

    m_maxRemoteArchiveSynchronizationThreads = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxRemoteArchiveSynchronizationThreads,
        kMaxRemoteArchiveSynchronizationThreadsDefault,
        this);

    m_targetUpdateInformationAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        kTargetUpdateInformationName,
        QByteArray(),
        this);

    m_installedUpdateInformationAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        kInstalledUpdateInformationName,
        QByteArray(),
        this);

    m_downloaderPeersAdaptor = new QnJsonResourcePropertyAdaptor<FileToPeerList>(
        kDownloaderPeersName,
        FileToPeerList(),
        this);

    m_maxWearableArchiveSynchronizationThreads = new QnLexicalResourcePropertyAdaptor<int>(
        kMaxWearableArchiveSynchronizationThreads,
        kMaxWearableArchiveSynchronizationThreadsDefault,
        this);

    m_watermarkSettingsAdaptor = new QnJsonResourcePropertyAdaptor<QnWatermarkSettings>(
        kWatermarkSettingsName,
        QnWatermarkSettings(),
        this);

    m_sessionTimeoutLimitMinutesAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        kSessionLimit,
        0,
        this);

    m_defaultVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kDefaultVideoCodec,
        "h263p",
        this);

    m_defaultExportVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kDefaultExportVideoCodec,
        "mpeg4",
        this);

    m_lowQualityScreenVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kLowQualityScreenVideoCodec,
        "mpeg2video",
        this);

    m_licenseServerUrlAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "licenseServer",
        "https://licensing.vmsproxy.com",
        this);

    m_resourceFileUriAdaptor = new QnLexicalResourcePropertyAdaptor<nx::utils::Url>(
        "resourceFileUri", "http://resources.vmsproxy.com/resource_data.json", this);

    m_maxEventLogRecordsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxEventLogRecords",
        100 * 1000, //< Default value.
        this);

    m_forceLiveCacheForPrimaryStreamAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        kForceLiveCacheForPrimaryStream,
        "auto",
        this);

    m_metadataStorageChangePolicyAdaptor = new QnLexicalResourcePropertyAdaptor<nx::vms::api::MetadataStorageChangePolicy>(
        kMetadataStorageChangePolicyName,
        nx::vms::api::MetadataStorageChangePolicy::keep,
        this);

    m_specificFeaturesAdaptor = new QnJsonResourcePropertyAdaptor<std::map<QString, int>>(
        kNameSpecificFeatures,
        {},
        this);

    connect(
        m_systemNameAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::systemNameChanged,
        Qt::QueuedConnection);

    connect(
        m_localSystemIdAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::localSystemIdChanged,
        Qt::QueuedConnection);

    // TODO: #GDM Revert this hack when sync global settings will be implemented.
    connect(
        m_localSystemIdAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::localSystemIdChangedDirect,
        Qt::DirectConnection);

    connect(
        m_disabledVendorsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::disabledVendorsChanged,
        Qt::QueuedConnection);

    connect(
        m_auditTrailEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::auditTrailEnableChanged,
        Qt::QueuedConnection);

    connect(
        m_auditTrailPeriodDaysAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::auditTrailPeriodDaysChanged,
        Qt::QueuedConnection);

    connect(
        m_trafficEncryptionForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::trafficEncryptionForcedChanged,
        Qt::QueuedConnection);

    connect(
        m_videoTrafficEncryptionForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::videoTrafficEncryptionForcedChanged,
        Qt::QueuedConnection);

    connect(
        m_eventLogPeriodDaysAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::eventLogPeriodDaysChanged,
        Qt::QueuedConnection);

    connect(
        m_cameraSettingsOptimizationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::cameraSettingsOptimizationChanged,
        Qt::QueuedConnection);

    connect(
        m_autoUpdateThumbnailsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::autoUpdateThumbnailsChanged,
        Qt::QueuedConnection);

    connect(
        m_maxSceneItemsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::maxSceneItemsChanged,
        Qt::DirectConnection); //< I need this one now :)

    connect(
        m_useTextEmailFormatAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::useTextEmailFormatChanged,
        Qt::QueuedConnection);

    connect(
        m_useWindowsEmailLineFeedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::useWindowsEmailLineFeedChanged,
        Qt::QueuedConnection);

    connect(
        m_autoDiscoveryEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::autoDiscoveryChanged,
        Qt::QueuedConnection);

    connect(
        m_autoDiscoveryResponseEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::autoDiscoveryChanged,
        Qt::QueuedConnection);

    connect(
        m_updateNotificationsEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::updateNotificationsChanged,
        Qt::QueuedConnection);

    connect(
        m_upnpPortMappingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::upnpPortMappingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudConnectUdpHolePunchingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::cloudConnectUdpHolePunchingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudConnectRelayingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::cloudConnectRelayingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        m_watermarkSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::watermarkChanged,
        Qt::QueuedConnection);

    connect(
        m_sessionTimeoutLimitMinutesAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::sessionTimeoutChanged,
        Qt::QueuedConnection);

    connect(
        m_targetUpdateInformationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::targetUpdateInformationChanged,
        Qt::QueuedConnection);

    connect(
        m_installedUpdateInformationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::installedUpdateInformationChanged,
        Qt::QueuedConnection);

    connect(
        m_downloaderPeersAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::downloaderPeersChanged,
        Qt::QueuedConnection);

    connect(
        m_resourceFileUriAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &QnGlobalSettings::resourceFileUriChanged,
        Qt::QueuedConnection);

    AdaptorList result;
    result
        << m_systemNameAdaptor
        << m_localSystemIdAdaptor
        << m_lastMergeMasterIdAdaptor
        << m_lastMergeSlaveIdAdaptor
        << m_disabledVendorsAdaptor
        << m_cameraSettingsOptimizationAdaptor
        << m_autoUpdateThumbnailsAdaptor
        << m_maxSceneItemsAdaptor
        << m_useTextEmailFormatAdaptor
        << m_useWindowsEmailLineFeedAdaptor
        << m_auditTrailEnabledAdaptor
        << m_auditTrailPeriodDaysAdaptor
        << m_trafficEncryptionForcedAdaptor
        << m_videoTrafficEncryptionForcedAdaptor
        << m_eventLogPeriodDaysAdaptor
        << m_autoDiscoveryEnabledAdaptor
        << m_autoDiscoveryResponseEnabledAdaptor
        << m_updateNotificationsEnabledAdaptor
        << m_backupQualitiesAdaptor
        << m_backupNewCamerasByDefaultAdaptor
        << m_upnpPortMappingEnabledAdaptor
        << m_cloudHostAdaptor
        << m_arecontRtspEnabledAdaptor
        << m_sequentialFlirOnvifSearcherEnabledAdaptor
        << m_maxP2pQueueSizeBytes
        << m_maxP2pQueueSizeForAllClientsBytes
        << m_maxRecorderQueueSizeBytes
        << m_maxRecorderQueueSizePackets
        << m_rtpFrameTimeoutMs
        << m_maxRtspConnectDuration
        << m_cloudConnectUdpHolePunchingEnabledAdaptor
        << m_cloudConnectRelayingEnabledAdaptor
        << m_edgeRecordingEnabledAdaptor
        << m_webSocketEnabledAdaptor
        << m_maxRemoteArchiveSynchronizationThreads
        << m_targetUpdateInformationAdaptor
        << m_installedUpdateInformationAdaptor
        << m_maxWearableArchiveSynchronizationThreads
        << m_watermarkSettingsAdaptor
        << m_sessionTimeoutLimitMinutesAdaptor
        << m_defaultVideoCodecAdaptor
        << m_defaultExportVideoCodecAdaptor
        << m_downloaderPeersAdaptor
        << m_lowQualityScreenVideoCodecAdaptor
        << m_licenseServerUrlAdaptor
        << m_resourceFileUriAdaptor
        << m_maxWebMTranscoders
        << m_maxEventLogRecordsAdaptor
        << m_forceLiveCacheForPrimaryStreamAdaptor
        << m_metadataStorageChangePolicyAdaptor
        << m_maxRtpRetryCount
        << m_specificFeaturesAdaptor
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

bool QnGlobalSettings::isUseWindowsEmailLineFeed() const
{
    return m_useWindowsEmailLineFeedAdaptor->value();
}

void QnGlobalSettings::setUseWindowsEmailLineFeed(bool value)
{
    m_useWindowsEmailLineFeedAdaptor->setValue(value);
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

bool QnGlobalSettings::isTrafficEncriptionForced() const
{
    return m_trafficEncryptionForcedAdaptor->value();
}

void QnGlobalSettings::setTrafficEncriptionForced(bool value)
{
    m_trafficEncryptionForcedAdaptor->setValue(value);
}

bool QnGlobalSettings::isVideoTrafficEncriptionForced() const
{
    return m_videoTrafficEncryptionForcedAdaptor->value();
}

void QnGlobalSettings::setVideoTrafficEncryptionForced(bool value)
{
    m_videoTrafficEncryptionForcedAdaptor->setValue(value);
}

bool QnGlobalSettings::isAutoDiscoveryEnabled() const
{
    return m_autoDiscoveryEnabledAdaptor->value();
}

void QnGlobalSettings::setAutoDiscoveryEnabled(bool enabled)
{
    m_autoDiscoveryEnabledAdaptor->setValue(enabled);
}

bool QnGlobalSettings::isAutoDiscoveryResponseEnabled() const
{
    return m_autoDiscoveryResponseEnabledAdaptor->value();
}

void QnGlobalSettings::setAutoDiscoveryResponseEnabled(bool enabled)
{
    m_autoDiscoveryResponseEnabledAdaptor->setValue(enabled);
}

void QnGlobalSettings::at_adminUserAdded(const QnResourcePtr& resource)
{
    if (m_admin)
        return;

    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    // TODO: Refactor this. Global settings should be moved from admin user to another place.
    if (!user->isBuiltInAdmin())
        return;

    {
        QnMutexLocker locker(&m_mutex);

        m_admin = user;
        for (auto adaptor: m_allAdaptors)
            adaptor->setResource(user);
    }
    emit initialized();
}

void QnGlobalSettings::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    if (!m_admin || resource != m_admin)
        return;

    QnMutexLocker locker( &m_mutex );
    m_admin.reset();

    for (auto adaptor: m_allAdaptors)
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
    result.searchTimeoutS = m_ldapSearchTimeoutSAdaptor->value();
    return result;
}

void QnGlobalSettings::setLdapSettings(const QnLdapSettings& settings)
{
    m_ldapUriAdaptor->setValue(settings.uri);
    m_ldapAdminDnAdaptor->setValue(settings.adminDn);
    m_ldapAdminPasswordAdaptor->setValue(
        settings.isValid()
        ? nx::utils::encodeHexStringFromStringAES128CBC(settings.adminPassword)
        : QString());
    m_ldapSearchBaseAdaptor->setValue(settings.searchBase);
    m_ldapSearchFilterAdaptor->setValue(settings.searchFilter);
    m_ldapSearchTimeoutSAdaptor->setValue(settings.searchTimeoutS);
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
    return result;
}

void QnGlobalSettings::setEmailSettings(const QnEmailSettings& settings)
{
    m_serverAdaptor->setValue(settings.server);
    m_fromAdaptor->setValue(settings.email);
    m_portAdaptor->setValue(settings.port);
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
    for (auto adaptor: m_allAdaptors)
        adaptor->saveToResource();

    QnMutexLocker locker(&m_mutex);
    if (!m_admin)
        return;

    resourcePropertyDictionary()->saveParamsAsync(m_admin->getId());
}

bool QnGlobalSettings::resynchronizeNowSync()
{
    {
        QnMutexLocker locker(&m_mutex);
        NX_ASSERT(m_admin, "Invalid sync state");
        if (!m_admin)
            return false;
        resourcePropertyDictionary()->markAllParamsDirty(m_admin->getId());
    }
    return synchronizeNowSync();
}

bool QnGlobalSettings::synchronizeNowSync()
{
    for (auto adaptor : m_allAdaptors)
        adaptor->saveToResource();

    QnMutexLocker locker(&m_mutex);
    NX_ASSERT(m_admin, "Invalid sync state");
    if (!m_admin)
        return false;
    return resourcePropertyDictionary()->saveParams(m_admin->getId());
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
        static const QString kStatisticsReportAllowed = "statisticsReportAllowed";
        // If user didn't make the decision in the current version, check if he made it in the
        // previous version.
        if (!isStatisticsAllowedDefined() && mediaServer &&
            !mediaServer->getProperty(kStatisticsReportAllowed).isEmpty())
        {
            bool value;
            if (QnLexical::deserialize(mediaServer->getProperty(kStatisticsReportAllowed), &value))
            {
                changed = true;
                m_statisticsAllowedAdaptor->setValue(QnOptionalBool(value));
            }
            mediaServer->setProperty(kStatisticsReportAllowed, QString());
            mediaServer->saveProperties();
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

void QnGlobalSettings::setBackupQualities(Qn::CameraBackupQualities value)
{
    m_backupQualitiesAdaptor->setValue(value);
}

bool QnGlobalSettings::backupNewCamerasByDefault() const
{
    return m_backupNewCamerasByDefaultAdaptor->value();
}

void QnGlobalSettings::setBackupNewCamerasByDefault(bool value)
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

void QnGlobalSettings::setStatisticsAllowed(bool value)
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
    NX_VERBOSE(this, lm("Providing local system id %1")
        .args(m_localSystemIdAdaptor->value()));

    return QnUuid(m_localSystemIdAdaptor->value());
}

void QnGlobalSettings::setLocalSystemId(const QnUuid& value)
{
    NX_DEBUG(this, lm("Changing local system id from %1 to %2")
        .args(m_localSystemIdAdaptor->value(), value));

    m_localSystemIdAdaptor->setValue(value.toString());
}

QnUuid QnGlobalSettings::lastMergeMasterId() const
{
    return QnUuid(m_lastMergeMasterIdAdaptor->value());
}

void QnGlobalSettings::setLastMergeMasterId(const QnUuid& value)
{
    m_lastMergeMasterIdAdaptor->setValue(value.toString());
}

QnUuid QnGlobalSettings::lastMergeSlaveId() const
{
    return QnUuid(m_lastMergeSlaveIdAdaptor->value());
}

void QnGlobalSettings::setLastMergeSlaveId(const QnUuid& value)
{
    m_lastMergeSlaveIdAdaptor->setValue(value.toString());
}

QString QnGlobalSettings::clientStatisticsSettingsUrl() const
{
    return m_clientStatisticsSettingsUrlAdaptor->value();
}

QString QnGlobalSettings::statisticsReportServerApi() const
{
    return m_statisticsReportServerApiAdaptor->value();
}

void QnGlobalSettings::setStatisticsReportServerApi(const QString& value)
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

int QnGlobalSettings::maxEventLogRecords() const
{
    return m_maxEventLogRecordsAdaptor->value();
}

void QnGlobalSettings::setMaxEventLogRecords(int value)
{
    m_maxEventLogRecordsAdaptor->setValue(value);
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

QnUuid QnGlobalSettings::primaryTimeServer() const
{
    return m_primaryTimeServerAdaptor->value();
}

void QnGlobalSettings::setPrimaryTimeServer(const QnUuid& value)
{
    m_primaryTimeServerAdaptor->setValue(value);
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

std::chrono::milliseconds QnGlobalSettings::osTimeChangeCheckPeriod() const
{
    return std::chrono::milliseconds(m_osTimeChangeCheckPeriodAdaptor->value());
}

void QnGlobalSettings::setOsTimeChangeCheckPeriod(std::chrono::milliseconds value)
{
    m_osTimeChangeCheckPeriodAdaptor->setValue(value.count());
}

std::chrono::milliseconds QnGlobalSettings::syncTimeExchangePeriod() const
{
    return std::chrono::milliseconds(m_syncTimeExchangePeriodAdaptor->value());
}

void QnGlobalSettings::setSyncTimeExchangePeriod(std::chrono::milliseconds value)
{
    m_syncTimeExchangePeriodAdaptor->setValue(value.count());
}

std::chrono::milliseconds QnGlobalSettings::syncTimeEpsilon() const
{
    return std::chrono::milliseconds(m_syncTimeEpsilonAdaptor->value());
}

void QnGlobalSettings::setSyncTimeEpsilon(std::chrono::milliseconds value)
{
    m_syncTimeEpsilonAdaptor->setValue(value.count());
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

std::chrono::seconds QnGlobalSettings::maxRtspConnectDuration() const
{
    return std::chrono::seconds(m_maxRtspConnectDuration->value());
}

void QnGlobalSettings::setMaxRtspConnectDuration(std::chrono::seconds newValue)
{
    m_maxRtspConnectDuration->setValue(newValue.count());
}

int QnGlobalSettings::maxP2pQueueSizeBytes() const
{
    return m_maxP2pQueueSizeBytes->value();
}

qint64 QnGlobalSettings::maxP2pQueueSizeForAllClientsBytes() const
{
    return m_maxP2pQueueSizeForAllClientsBytes->value();
}

int QnGlobalSettings::maxRecorderQueueSizeBytes() const
{
    return m_maxRecorderQueueSizeBytes->value();
}

int QnGlobalSettings::maxWebMTranscoders() const
{
    return m_maxWebMTranscoders->value();
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

QByteArray QnGlobalSettings::targetUpdateInformation() const
{
    return m_targetUpdateInformationAdaptor->value();
}

bool QnGlobalSettings::isWebSocketEnabled() const
{
    return m_webSocketEnabledAdaptor->value();
}

void QnGlobalSettings::setWebSocketEnabled(bool enabled)
{
    m_webSocketEnabledAdaptor->setValue(enabled);
}

void QnGlobalSettings::setTargetUpdateInformation(const QByteArray& updateInformation)
{
    m_targetUpdateInformationAdaptor->setValue(updateInformation);
}

QByteArray QnGlobalSettings::installedUpdateInformation() const
{
    return m_installedUpdateInformationAdaptor->value();
}

void QnGlobalSettings::setInstalledUpdateInformation(const QByteArray& updateInformation)
{
    m_installedUpdateInformationAdaptor->setValue(updateInformation);
}

FileToPeerList QnGlobalSettings::downloaderPeers() const
{
    return m_downloaderPeersAdaptor->value();
}

void QnGlobalSettings::setdDownloaderPeers(const FileToPeerList& downloaderPeers)
{
    m_downloaderPeersAdaptor->setValue(downloaderPeers);
}

int QnGlobalSettings::maxRemoteArchiveSynchronizationThreads() const
{
    return m_maxRemoteArchiveSynchronizationThreads->value();
}

void QnGlobalSettings::setMaxRemoteArchiveSynchronizationThreads(int newValue)
{
    m_maxRemoteArchiveSynchronizationThreads->setValue(newValue);
}

int QnGlobalSettings::maxWearableArchiveSynchronizationThreads() const
{
    return m_maxWearableArchiveSynchronizationThreads->value();
}

void QnGlobalSettings::setMaxWearableArchiveSynchronizationThreads(int newValue)
{
    m_maxWearableArchiveSynchronizationThreads->setValue(newValue);
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

QnWatermarkSettings QnGlobalSettings::watermarkSettings() const
{
    return m_watermarkSettingsAdaptor->value();
}

void QnGlobalSettings::setWatermarkSettings(const QnWatermarkSettings& settings) const
{
    m_watermarkSettingsAdaptor->setValue(settings);
}

std::chrono::minutes QnGlobalSettings::sessionTimeoutLimit() const
{
    return std::chrono::minutes(m_sessionTimeoutLimitMinutesAdaptor->value());
}

void QnGlobalSettings::setSessionTimeoutLimit(std::chrono::minutes value)
{
    m_sessionTimeoutLimitMinutesAdaptor->setValue(value.count());
}

QString QnGlobalSettings::defaultVideoCodec() const
{
    return m_defaultVideoCodecAdaptor->value();
}

void QnGlobalSettings::setDefaultVideoCodec(const QString& value)
{
    m_defaultVideoCodecAdaptor->setValue(value);
}

QString QnGlobalSettings::defaultExportVideoCodec() const
{
    return m_defaultExportVideoCodecAdaptor->value();
}

void QnGlobalSettings::setDefaultExportVideoCodec(const QString& value)
{
    m_defaultExportVideoCodecAdaptor->setValue(value);
}

QString QnGlobalSettings::lowQualityScreenVideoCodec() const
{
    return m_lowQualityScreenVideoCodecAdaptor->value();
}

void QnGlobalSettings::setLicenseServerUrl(const QString& value)
{
    m_licenseServerUrlAdaptor->setValue(value);
}

QString QnGlobalSettings::licenseServerUrl() const
{
    return m_licenseServerUrlAdaptor->value();
}

void QnGlobalSettings::setResourceFileUri(const nx::utils::Url& value)
{
    m_resourceFileUriAdaptor->setValue(value);
}

nx::utils::Url QnGlobalSettings::resourceFileUri() const
{
    return m_resourceFileUriAdaptor->value();
}

void QnGlobalSettings::setLowQualityScreenVideoCodec(const QString& value)
{
    m_lowQualityScreenVideoCodecAdaptor->setValue(value);
}

QString QnGlobalSettings::forceLiveCacheForPrimaryStream() const
{
    return m_forceLiveCacheForPrimaryStreamAdaptor->value();
}

void QnGlobalSettings::setForceLiveCacheForPrimaryStream(const QString& value)
{
    m_forceLiveCacheForPrimaryStreamAdaptor->setValue(value);
}

const QList<QnAbstractResourcePropertyAdaptor*>& QnGlobalSettings::allSettings() const
{
    return m_allAdaptors;
}

bool QnGlobalSettings::isGlobalSetting(const nx::vms::api::ResourceParamWithRefData& param)
{
    return QnUserResource::kAdminGuid == param.resourceId;
}

nx::vms::api::MetadataStorageChangePolicy QnGlobalSettings::metadataStorageChangePolicy() const
{
    return m_metadataStorageChangePolicyAdaptor->value();
}

void QnGlobalSettings::setMetadataStorageChangePolicy(nx::vms::api::MetadataStorageChangePolicy value)
{
    m_metadataStorageChangePolicyAdaptor->setValue(value);
}

std::map<QString, int> QnGlobalSettings::specificFeatures() const
{
    return m_specificFeaturesAdaptor->value();
}

void QnGlobalSettings::setSpecificFeatures(std::map<QString, int> value)
{
    m_specificFeaturesAdaptor->setValue(value);
}
