// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_settings.h"

#include <chrono>

#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/branding.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/api/data/email_settings.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/api/data/watermark_settings.h>
#include <nx/vms/common/system_context.h>
#include <utils/email/email.h>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

static constexpr seconds kDefaultSessionLimit = 30 * 24h;

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

    return QSet(updatedVendorList.begin(), updatedVendorList.end());
}

} // namespace

namespace nx::vms::common {

SystemSettings::SystemSettings(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context)
{
    m_allAdaptors
        << initEmailAdaptors()
        << initStaticticsAdaptors()
        << initConnectionAdaptors()
        << initTimeSynchronizationAdaptors()
        << initCloudAdaptors()
        << initMiscAdaptors()
        ;

    for (const auto& adapter: m_allAdaptors)
    {
        if (Names::kReadOnlyNames.contains(adapter->key()))
            adapter->markReadOnly();
        if (Names::kWriteOnlyNames.contains(adapter->key()))
            adapter->markWriteOnly();
        if (Names::kSecurityNames.contains(adapter->key()))
            adapter->markSecurity();
        if (Names::kHiddenNames.contains(adapter->key()))
        {
            adapter->markReadOnly();
            adapter->markWriteOnly();
        }
    }

    connect(resourcePool(), &QnResourcePool::resourcesAdded, this, &SystemSettings::initialize,
        Qt::DirectConnection);

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &SystemSettings::at_resourcePool_resourceRemoved, Qt::DirectConnection);

    initialize();
}

SystemSettings::~SystemSettings()
{
}

void SystemSettings::initialize()
{
    if (isInitialized())
        return;

    // TODO: #rvasilenko Refactor this. System settings should be moved out from admin user.
    const auto user = resourcePool()->getAdministrator();
    if (!user)
        return;

    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        NX_VERBOSE(this, "System settings successfully initialized");
        m_admin = user;
        for (auto adaptor: m_allAdaptors)
            adaptor->setResource(user);
    }
    emit initialized();
}

bool SystemSettings::isInitialized() const
{
    return !m_admin.isNull();
}

SystemSettings::AdaptorList SystemSettings::initEmailAdaptors()
{
    const auto isValid =
        [](const nx::vms::api::EmailSettings& s)
        {
            return !s.useCloudServiceToSendEmail
                || s.supportAddress == nx::branding::supportAddress();
        };

    m_emailSettingsAdaptor =
        new QnJsonResourcePropertyAdaptor<nx::vms::api::EmailSettings>(
            "emailSettings",
            nx::vms::api::EmailSettings{},
            isValid,
            this,
            [] { return tr("SMTP settings"); });

    connect(
        m_emailSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::emailSettingsChanged,
        Qt::QueuedConnection);

    return {m_emailSettingsAdaptor};
}

SystemSettings::AdaptorList SystemSettings::initStaticticsAdaptors()
{
    m_statisticsAllowedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "statisticsAllowed", true, this,
        [] { return tr("Anonymous statistics report allowed"); });

    m_statisticsReportLastTimeAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportLastTime", QString(), this,
        [] { return tr("Anonymous statistics report last time"); });

    m_statisticsReportLastVersionAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportLastVersion", QString(), this,
        [] { return tr("Anonymous statistics report last version"); });

    m_statisticsReportLastNumberAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "statisticsReportLastNumber", 0, this,
        [] { return tr("Anonymous statistics report last number"); });

    m_statisticsReportTimeCycleAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportTimeCycle", QString(), this,
        [] { return tr("Anonymous statistics time cycle"); });

    m_statisticsReportUpdateDelayAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportUpdateDelay", QString(), this,
        [] { return tr("Anonymous statistics report delay after update"); });

    m_statisticsReportServerApiAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportServerApi", QString(), this,
        [] { return tr("Anonymous Statistics Report Server URL"); });

    m_clientStatisticsSettingsUrlAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "clientStatisticsSettingsUrl", QString(), this,
        [] { return tr("Anonymous statistics report Client settings"); });

    connect(
        m_statisticsAllowedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::statisticsAllowedChanged,
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

SystemSettings::AdaptorList SystemSettings::initConnectionAdaptors()
{
    AdaptorList ec2Adaptors;

    m_ec2AliveUpdateIntervalAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "ec2AliveUpdateIntervalSec", 60,
        [](auto v) { return v >= 1 && v <= 60 * 60; }, this,
        [] { return tr("System alive update interval (seconds, 1s-1h)"); });
    ec2Adaptors << m_ec2AliveUpdateIntervalAdaptor;

    m_proxyConnectTimeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "proxyConnectTimeoutSec", 5,
        [](auto v) { return v >= 1 && v <= 60 * 60; }, this,
        [] { return tr("Proxy connection timeout (seconds, 1s-1h)"); });
    ec2Adaptors << m_proxyConnectTimeoutAdaptor;

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

SystemSettings::AdaptorList SystemSettings::initTimeSynchronizationAdaptors()
{
    using namespace std::chrono;

    QList<QnAbstractResourcePropertyAdaptor*> timeSynchronizationAdaptors;

    m_timeSynchronizationEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "timeSynchronizationEnabled", true, this,
        [] { return tr("Time synchronization enabled"); });
    timeSynchronizationAdaptors << m_timeSynchronizationEnabledAdaptor;

    m_primaryTimeServerAdaptor = new QnLexicalResourcePropertyAdaptor<QnUuid>(
        Names::primaryTimeServer, QnUuid(), this,
        [] { return tr("Primary time synchronization Server ID"); });
    timeSynchronizationAdaptors << m_primaryTimeServerAdaptor;

    m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "maxDifferenceBetweenSynchronizedAndInternetTime",
            duration_cast<milliseconds>(std::chrono::seconds(2)).count(),
            this,
            [] { return tr("Max difference between local and source time (milliseconds)"); });
    timeSynchronizationAdaptors << m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor;

    m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "maxDifferenceBetweenSynchronizedAndLocalTimeMs",
            duration_cast<milliseconds>(std::chrono::seconds(2)).count(),
            this,
            [] { return tr("Max difference between local and source time (milliseconds)"); });
    timeSynchronizationAdaptors << m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor;

    m_osTimeChangeCheckPeriodAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "osTimeChangeCheckPeriodMs",
            duration_cast<milliseconds>(std::chrono::seconds(5)).count(),
            this,
            [] { return tr("OS time change check period"); });
    timeSynchronizationAdaptors << m_osTimeChangeCheckPeriodAdaptor;

    m_syncTimeExchangePeriodAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "syncTimeExchangePeriod",
            duration_cast<milliseconds>(std::chrono::minutes(10)).count(),
            this,
            [] { return tr("Sync time synchronization interval for network requests"); });
    timeSynchronizationAdaptors << m_syncTimeExchangePeriodAdaptor;

    m_syncTimeEpsilonAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "syncTimeEpsilon",
            duration_cast<milliseconds>(std::chrono::milliseconds(200)).count(),
            this,
            [] { return tr("Sync time epsilon. New value is not applied if time delta less than epsilon"); });
    timeSynchronizationAdaptors << m_syncTimeEpsilonAdaptor;

    for (auto adaptor: timeSynchronizationAdaptors)
    {
        connect(
            adaptor,
            &QnAbstractResourcePropertyAdaptor::valueChanged,
            this,
            &SystemSettings::timeSynchronizationSettingsChanged,
            Qt::QueuedConnection);
    }

    return timeSynchronizationAdaptors;
}

SystemSettings::AdaptorList SystemSettings::initCloudAdaptors()
{
    m_cloudAccountNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudAccountName, QString(), this, [] { return tr("Cloud owner account"); });

    m_organizationIdAdaptor = new QnLexicalResourcePropertyAdaptor<QnUuid>(
        Names::organizationId, QnUuid(), this, [] { return tr("Organization Id"); });

    m_cloudSystemIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudSystemID, QString(), this, [] { return tr("Cloud System ID"); });

    m_cloudAuthKeyAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudAuthKey, QString(), this, [] { return tr("Cloud authorization key"); });

    m_system2faEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::system2faEnabled, false, this, [] { return tr("Enable 2FA for the System"); });

    m_masterCloudSyncAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "masterCloudSyncList", QString(), this,
        [] { return tr("Semicolon-separated list of Servers designated to connect to the Cloud. "
            "Servers at the top of the list have higher priority. If the list is empty a Server "
            "for the Cloud connection is selected automatically."); });


    AdaptorList result;
    result
        << m_cloudAccountNameAdaptor
        << m_organizationIdAdaptor
        << m_cloudSystemIdAdaptor
        << m_cloudAuthKeyAdaptor
        << m_system2faEnabledAdaptor
        << m_masterCloudSyncAdaptor;

    for (auto adaptor: result)
    {
        connect(
            adaptor,
            &QnAbstractResourcePropertyAdaptor::valueChanged,
            this,
            &SystemSettings::cloudSettingsChanged,
            Qt::QueuedConnection);
    }

    connect(
        m_cloudSystemIdAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudAuthKeyAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);

    return result;
}

SystemSettings::AdaptorList SystemSettings::initMiscAdaptors()
{
    using namespace std::chrono;

    m_systemNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::systemName, QString(), this, [] { return tr("System name"); });

    m_localSystemIdAdaptor = new QnLexicalResourcePropertyAdaptor<QnUuid>(
        Names::localSystemId, QnUuid(), this,
        [] { return tr("Local System ID, null means the System is not set up yet."); });

    m_lastMergeMasterIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::lastMergeMasterId, QString(), this, [] { return tr("Last master System merge ID"); });

    m_lastMergeSlaveIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::lastMergeSlaveId, QString(), this, [] { return tr("Last slave System merge ID"); });

    m_disabledVendorsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::disabledVendors, QString(), this, [] { return tr("Disable Device vendors"); });

    m_cameraSettingsOptimizationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cameraSettingsOptimization", true, this, [] { return tr("Optimize Camera settings"); });

    m_autoUpdateThumbnailsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "autoUpdateThumbnails", true, this, [] { return tr("Thumbnails auto-update"); });

    m_maxSceneItemsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxSceneItems", 0, this, [] { return tr("Max scene items (0 means default)"); });

    m_useTextEmailFormatAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "useTextEmailFormat", false, this, [] { return tr("Send plain-text emails"); });

    m_useWindowsEmailLineFeedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "useWindowsEmailLineFeed", false, this,
        [] { return tr("Use Windows line feed in emails"); });

    m_auditTrailEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "auditTrailEnabled", true, this, [] { return tr("Enable audit trail"); });

    m_auditTrailPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "auditTrailPeriodDays", 183,
        [](auto v) { return v >= 14 && v <= 730; }, this,
        [] { return tr("Audit trail period (days, 14-730)"); });

    m_eventLogPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "eventLogPeriodDays", 30, this, [] { return tr("Event log period (days)"); });

    m_trafficEncryptionForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::trafficEncryptionForced, true, this,
        [] { return tr("Enforce HTTPS (data traffic encryption)"); });

    m_videoTrafficEncryptionForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::videoTrafficEncryptionForced, false, this,
        [] { return tr("Enforce RTSPS (video traffic encryption)"); });

    m_exposeDeviceCredentialsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::exposeDeviceCredentials, true, this,
        [] { return tr("Expose device passwords stored in VMS for administrators (for web pages)"); });

    m_autoDiscoveryEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "autoDiscoveryEnabled", true, this,
        [] { return tr("Enable auto-discovery"); });

    m_autoDiscoveryResponseEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "autoDiscoveryResponseEnabled", true, this,
        [] { return tr("Enable auto-update notifications"); });

    m_updateNotificationsEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "updateNotificationsEnabled", true, this, [] { return tr("Enable update notifications"); });

     m_upnpPortMappingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "upnpPortMappingEnabled", true, this, [] { return tr("Enable UPNP port-mapping"); });

    m_backupSettingsAdaptor = new QnJsonResourcePropertyAdaptor<nx::vms::api::BackupSettings>(
        Names::backupSettings, nx::vms::api::BackupSettings(), this,
        [] { return tr("Backup settings"); });

    m_cloudHostAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudHost, QString(), this, [] { return tr("Cloud host override"); });

    m_crossdomainEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "crossdomainEnabled", false, this, [] { return tr("Enable cross-domain policy"); });

    m_arecontRtspEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "arecontRtspEnabled", false, this, [] { return tr("Enable RTSP for Arecont"); });

    m_sequentialFlirOnvifSearcherEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "sequentialFlirOnvifSearcherEnabled", false, this,
        [] { return tr("Enable sequential Flir ONVIF searcher"); });

    m_maxP2pQueueSizeBytes = new QnLexicalResourcePropertyAdaptor<int>(
        "maxP2pQueueSizeBytes", 1024 * 1024 * 128,
        [](auto v) { return v >= 1024 * 1024 * 32 && v <= 1024 * 1024 * 512; }, this,
        [] { return tr("Max P2P queue size (bytes, 32-512MB)"); });

    m_maxP2pQueueSizeForAllClientsBytes = new QnLexicalResourcePropertyAdaptor<qint64>(
        "maxP2pAllClientsSizeBytes", 1024 * 1024 * 128,
        [](auto v) { return v >= 1024 * 1024 * 32 && v <= 1024 * 1024 * 512; }, this,
        [] { return tr("Max P2P all clients size (bytes, 32-512MB)"); });

    m_maxRecorderQueueSizeBytes = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRecordQueueSizeBytes", 1024 * 1024 * 24,
        [](auto v) { return v >= 1024 * 1024 * 6 && v <= 1024 * 1024 * 96; }, this,
        [] { return tr("Max record queue size (bytes, 6-96MB)"); });

    m_maxRecorderQueueSizePackets = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRecordQueueSizeElements", 1000,
        [](auto v) { return v >= 250 && v <= 4000; }, this,
        [] { return tr("Max record queue size (elements, 250-4000)"); });

    m_maxHttpTranscodingSessions = new QnLexicalResourcePropertyAdaptor<int>(
        Names::maxHttpTranscodingSessions, 2, this,
        [] { return tr(
            "Max amount of HTTP connections using transcoding for the Server. Chrome opens 2 "
            "connections at once, then closes the first one. "
            "We recommend setting this parameter's value to 2 or more."); });

    m_maxRtpRetryCount = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRtpRetryCount", 6, this, [] { return tr(
            "Maximum number of consecutive RTP errors before the server reconnects the RTSP "
            "session."); });

    m_rtpFrameTimeoutMs = new QnLexicalResourcePropertyAdaptor<int>(
        "rtpTimeoutMs", 10'000, this, [] { return tr("RTP timeout (milliseconds)"); });

    m_maxRtspConnectDuration = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRtspConnectDurationSeconds", 0, this,
        [] { return tr("Max RTSP connection duration (seconds)"); });

    m_cloudConnectUdpHolePunchingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cloudConnectUdpHolePunchingEnabled", true, this,
        [] { return tr("Enable cloud-connect UDP hole-punching"); });

    m_cloudConnectRelayingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cloudConnectRelayingEnabled", true, this,
        [] { return tr("Enable cloud-connect relays usage"); });

    m_cloudConnectRelayingOverSslForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cloudConnectRelayingOverSslForced", false, this,
        [] { return tr("Enforce SSL for cloud-connect relays"); });

    m_edgeRecordingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "enableEdgeRecording", true, this, [] { return tr("Enable recording on EDGE"); });

    m_webSocketEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::webSocketEnabled, true, this, [] { return tr("Enable WebSocket for P2P"); });

    m_maxRemoteArchiveSynchronizationThreads = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRemoteArchiveSynchronizationThreads", -1,
        [](auto v) { return v >= -1 && v <= 32; }, this,
        [] { return tr("Max thread count for remote archive synchronization (<=0 - auto, max 32)"); });

    m_customReleaseListUrlAdaptor = new QnLexicalResourcePropertyAdaptor<utils::Url>(
        "customReleaseListUrl", utils::Url(), this,
        [] { return tr("Update releases.json file URL"); });

    m_targetUpdateInformationAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        "targetUpdateInformation", QByteArray(), this,
        [] { return tr("Target update information"); });

    m_installedUpdateInformationAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        "installedUpdateInformation", QByteArray(), this,
        [] { return tr("Installed update information"); });

    m_downloaderPeersAdaptor = new QnJsonResourcePropertyAdaptor<FileToPeerList>(
        "downloaderPeers", FileToPeerList(), this, [] { return tr("Downloader peers for files"); });

    m_clientUpdateSettingsAdaptor = new QnJsonResourcePropertyAdaptor<api::ClientUpdateSettings>(
        "clientUpdateSettings", api::ClientUpdateSettings{}, this,
        [] { return tr("Client update settings"); });

    m_maxVirtualCameraArchiveSynchronizationThreads = new QnLexicalResourcePropertyAdaptor<int>(
        "maxVirtualCameraArchiveSynchronizationThreads", -1, this,
        [] { return tr("Thread count limit for camera archive synchronization"); });

    m_watermarkSettingsAdaptor = new QnJsonResourcePropertyAdaptor<nx::vms::api::WatermarkSettings>(
        "watermarkSettings", nx::vms::api::WatermarkSettings(), this,
        [] { return tr("Watermark settings"); });

    m_sessionTimeoutLimitSecondsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::sessionLimitS,
        kDefaultSessionLimit.count(),
        [](const int& value) { return value >= 0; },
        this,
        [] { return tr("Authorization Session token lifetime (seconds)"); });

    m_sessionsLimitAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::sessionsLimit, 10'0000, [](const int& value) { return value >= 0; }, this,
        [] { return tr("Session token count limit on a single Server"); });

    m_sessionsLimitPerUserAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::sessionsLimitPerUser, 5'000, [](const int& value) { return value >= 0; }, this,
        [] { return tr("Max session token count per user on single Server"); });

    m_remoteSessionUpdateAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::remoteSessionUpdateS, 10, [](const int& value) { return value > 0; }, this,
        [] { return tr("Update interval for remote session token cache (other Servers and Cloud)"); });

    m_remoteSessionTimeoutAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::remoteSessionTimeoutS, 10 * 60, [](const int& value) { return value > 0; }, this,
        [] { return tr("Timeout for remote session token cache (other Servers and Cloud)"); });

    m_defaultVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "defaultVideoCodec", "h263p", this, [] { return tr("Default video codec"); });

    m_defaultExportVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "defaultExportVideoCodec", "mpeg4", this,
        [] { return tr("Default codec for export video"); });

    m_lowQualityScreenVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "lowQualityScreenVideoCodec", "mpeg2video", this,
        [] { return tr("Low quality screen video codec"); });

    m_licenseServerUrlAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::licenseServer, "https://licensing.vmsproxy.com", this,
        [] { return tr("License server"); });

    m_channelPartnerServerUrlAdaptor = new QnLexicalResourcePropertyAdaptor<utils::Url>(
        Names::channelPartnerServer, "https://partners.test.hdw.mx", this,
        [] { return tr("Channel partners service"); });

    m_resourceFileUriAdaptor = new QnLexicalResourcePropertyAdaptor<nx::utils::Url>(
        Names::resourceFileUri, "https://resources.vmsproxy.com/resource_data.json", this,
        [] { return tr("URI for resource_data.json automatic update"); });

    m_maxEventLogRecordsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxEventLogRecords", 100 * 1000, this,
        [] { return tr(
            "Maximum event log records to keep in the database. Real amount of undeleted records "
            "may be up to 20% higher than the specified value."); });

    m_forceLiveCacheForPrimaryStreamAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "forceLiveCacheForPrimaryStream", "auto", this,
        [] { return tr(
            "Whether or not to cache some frames for the primary stream. Values: "
            "'yes' - always enabled (may use a lot of RAM), "
            "'no' - always disabled except when required by the playback (e.g. HLS), "
            "'auto' - similar to 'no', but turned on when improving the user experience "
                "(e.g. when some Analytics plugin is working on the Camera)."); });

    m_metadataStorageChangePolicyAdaptor =
        new QnReflectLexicalResourcePropertyAdaptor<nx::vms::api::MetadataStorageChangePolicy>(
            "metadataStorageChangePolicy", nx::vms::api::MetadataStorageChangePolicy::keep, this,
            [] { return tr("Meta data storage change policy"); });

    m_targetPersistentUpdateStorageAdaptor =
        new QnJsonResourcePropertyAdaptor<nx::vms::common::update::PersistentUpdateStorage>(
            "targetPersistentUpdateStorage", nx::vms::common::update::PersistentUpdateStorage(), this,
            [] { return tr("Persistent Servers for update storage"); });

    m_installedPersistentUpdateStorageAdaptor =
        new QnJsonResourcePropertyAdaptor<nx::vms::common::update::PersistentUpdateStorage>(
            "installedPersistentUpdateStorage", nx::vms::common::update::PersistentUpdateStorage(), this,
            [] { return tr("Persistent Servers where updates are stored"); });

    m_specificFeaturesAdaptor = new QnJsonResourcePropertyAdaptor<std::map<QString, int>>(
        Names::specificFeatures, std::map<QString, int>{}, this,
        [] { return tr("VMS Server version specific features"); });

    m_cloudNotificationsLanguageAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudNotificationsLanguage, "", this,
        [] { return tr("Language for Cloud notifications"); });

    m_additionalLocalFsTypesAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "additionalLocalFsTypes", "", this,
        [] { return tr("Additional local FS storage types for recording"); });

    m_keepIoPortStateIntactOnInitializationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "keepIoPortStateIntactOnInitialization", false, this,
        [] { return tr("Keep IO port state on when Server connects to the device"); });

    m_mediaBufferSizeKbAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "mediaBufferSizeKb", 256,
        [](auto v) { return v >= 10 && v <= 4 * 1024; }, this,
        [] { return tr("Media buffer size (KB, 10KB-4MB)"); });

    m_mediaBufferSizeKbForAudioOnlyDeviceAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "mediaBufferSizeForAudioOnlyDeviceKb", 16,
        [](auto v) { return v >= 1 && v <= 1024; }, this,
        [] { return tr("Media buffer size for audio only devices (KB, 1KB-1MB)"); });

    m_forceAnalyticsDbStoragePermissionsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "forceAnalyticsDbStoragePermissions", true,  this,
        [] { return tr("Force analytics DB storage mount point permissions in case of failure"); });

    m_checkVideoStreamPeriodMsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "checkVideoStreamPeriodMs", 10'000, this,
        [] { return tr("Check video stream period (milliseconds)"); });

    m_useStorageEncryptionAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "storageEncryption", false, this, [] { return tr("Storage encryption enabled"); });

    m_currentStorageEncryptionKeyAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        "currentStorageEncryptionKey", QByteArray(), this,
        [] { return tr("Current storage encryption key"); });

    m_showServersInTreeForNonAdminsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "showServersInTreeForNonAdmins", true, this,
        [] { return tr("Show Servers in the Resource Tree for non-admins"); });

    m_supportedOriginsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
         Names::supportedOrigins, "*", this, [] { return tr("HTTP header: Origin"); });

    m_frameOptionsHeaderAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
         Names::frameOptionsHeader, "SAMEORIGIN", this, [] { return tr("HTTP header: X-Frame-Options"); });

    m_useHttpsOnlyForCamerasAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "useHttpsOnlyForCameras", false, this, [] { return tr("Use only HTTPS for cameras"); });

    m_securityForPowerUsersAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::securityForPowerUsers, true, this,
        [] { return tr("Allow Power User editing Security Settings"); });

    connect(m_securityForPowerUsersAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
        this, &SystemSettings::securityForPowerUsersChanged, Qt::QueuedConnection);

    m_insecureDeprecatedApiEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::insecureDeprecatedApiEnabled, false, this,
        [] { return tr("Enable deprecated API functions (insecure)"); });

    m_insecureDeprecatedApiInUseEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::insecureDeprecatedApiInUseEnabled, false, this,
        []
        {
            return nx::format(tr(
                "Enable deprecated API functions currently used by %1 software (insecure)",
                "%1 is a company name"),
                nx::branding::company());
        });

    m_exposeServerEndpointsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::exposeServerEndpoints, true, this,
        []() { return tr("Expose IP addresses for autodiscovery"); });

    m_showMouseTimelinePreviewAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "showMouseTimelinePreview", true, this, [] { return tr("Show mouse timeline preview"); });

    m_ldapAdaptor = new QnJsonResourcePropertyAdaptor<nx::vms::api::LdapSettings>(
        Names::ldap, nx::vms::api::LdapSettings(), this,
        [] { return tr("LDAP settings"); });

    m_cloudPollingIntervalAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::cloudPollingIntervalS, 60, [](const int& value) { return value >= 1; }, this,
        []
        {
            return tr("Interval between the Cloud polling HTTP requests to synchronize the data.");
        });

    connect(
        m_systemNameAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::systemNameChanged,
        Qt::QueuedConnection);

    connect(
        m_localSystemIdAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::localSystemIdChanged,
        Qt::DirectConnection);

    connect(
        m_disabledVendorsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::disabledVendorsChanged,
        Qt::QueuedConnection);

    connect(
        m_auditTrailEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::auditTrailEnableChanged,
        Qt::QueuedConnection);

    connect(
        m_auditTrailPeriodDaysAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::auditTrailPeriodDaysChanged,
        Qt::QueuedConnection);

    connect(
        m_trafficEncryptionForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::trafficEncryptionForcedChanged,
        Qt::QueuedConnection);

    connect(
        m_videoTrafficEncryptionForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::videoTrafficEncryptionForcedChanged,
        Qt::QueuedConnection);

    connect(
        m_eventLogPeriodDaysAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::eventLogPeriodDaysChanged,
        Qt::QueuedConnection);

    connect(
        m_cameraSettingsOptimizationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cameraSettingsOptimizationChanged,
        Qt::QueuedConnection);

    connect(
        m_useHttpsOnlyForCamerasAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useHttpsOnlyForCamerasChanged,
        Qt::QueuedConnection);

    connect(
        m_autoUpdateThumbnailsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::autoUpdateThumbnailsChanged,
        Qt::QueuedConnection);

    connect(
        m_maxSceneItemsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::maxSceneItemsChanged,
        Qt::DirectConnection); //< I need this one now :)

    connect(
        m_useTextEmailFormatAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useTextEmailFormatChanged,
        Qt::QueuedConnection);

    connect(
        m_useWindowsEmailLineFeedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useWindowsEmailLineFeedChanged,
        Qt::QueuedConnection);

    connect(
        m_autoDiscoveryEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::autoDiscoveryChanged,
        Qt::QueuedConnection);

    connect(
        m_autoDiscoveryResponseEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::autoDiscoveryChanged,
        Qt::QueuedConnection);

    connect(
        m_updateNotificationsEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::updateNotificationsChanged,
        Qt::QueuedConnection);

    connect(
        m_upnpPortMappingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::upnpPortMappingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudConnectUdpHolePunchingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudConnectUdpHolePunchingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudConnectRelayingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudConnectRelayingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudConnectRelayingOverSslForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudConnectRelayingOverSslForcedChanged,
        Qt::QueuedConnection);

    connect(
        m_watermarkSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::watermarkChanged,
        Qt::QueuedConnection);

    connect(
        m_sessionTimeoutLimitSecondsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::sessionTimeoutChanged,
        Qt::QueuedConnection);

    connect(
        m_sessionsLimitAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::sessionsLimitChanged,
        Qt::QueuedConnection);

    connect(
        m_sessionsLimitPerUserAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::sessionsLimitPerUserChanged,
        Qt::QueuedConnection);

    connect(
        m_remoteSessionUpdateAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::remoteSessionUpdateChanged,
        Qt::QueuedConnection);

    connect(
        m_remoteSessionTimeoutAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::remoteSessionUpdateChanged,
        Qt::QueuedConnection);

    connect(
        m_useStorageEncryptionAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useStorageEncryptionChanged,
        Qt::QueuedConnection);

    connect(
        m_currentStorageEncryptionKeyAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::currentStorageEncryptionKeyChanged,
        Qt::QueuedConnection);

    connect(
        m_showServersInTreeForNonAdminsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::showServersInTreeForNonAdmins,
        Qt::QueuedConnection);

    connect(
        m_customReleaseListUrlAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::customReleaseListUrlChanged,
        Qt::QueuedConnection);

    connect(
        m_targetUpdateInformationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::targetUpdateInformationChanged,
        Qt::QueuedConnection);

    connect(
        m_installedUpdateInformationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::installedUpdateInformationChanged,
        Qt::QueuedConnection);

    connect(
        m_downloaderPeersAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::downloaderPeersChanged,
        Qt::QueuedConnection);

    connect(
        m_targetPersistentUpdateStorageAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::targetPersistentUpdateStorageChanged,
        Qt::QueuedConnection);

    connect(
        m_installedPersistentUpdateStorageAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::installedPersistentUpdateStorageChanged,
        Qt::QueuedConnection);

    connect(
        m_clientUpdateSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::clientUpdateSettingsChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudNotificationsLanguageAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudNotificationsLanguageChanged,
        Qt::QueuedConnection);

    connect(
        m_backupSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::backupSettingsChanged,
        Qt::QueuedConnection);

    connect(
        m_showMouseTimelinePreviewAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::showMouseTimelinePreviewChanged,
        Qt::QueuedConnection);

    connect(
        m_ldapAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::ldapSettingsChanged,
        Qt::DirectConnection);

    connect(
        m_masterCloudSyncAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::masterCloudSyncChanged,
        Qt::QueuedConnection);

    connect(
        m_cloudPollingIntervalAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudPollingIntervalChanged,
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
        << m_exposeDeviceCredentialsAdaptor
        << m_useHttpsOnlyForCamerasAdaptor
        << m_securityForPowerUsersAdaptor
        << m_insecureDeprecatedApiEnabledAdaptor
        << m_insecureDeprecatedApiInUseEnabledAdaptor
        << m_eventLogPeriodDaysAdaptor
        << m_autoDiscoveryEnabledAdaptor
        << m_autoDiscoveryResponseEnabledAdaptor
        << m_updateNotificationsEnabledAdaptor
        << m_backupSettingsAdaptor
        << m_upnpPortMappingEnabledAdaptor
        << m_cloudHostAdaptor
        << m_crossdomainEnabledAdaptor
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
        << m_cloudConnectRelayingOverSslForcedAdaptor
        << m_edgeRecordingEnabledAdaptor
        << m_webSocketEnabledAdaptor
        << m_maxRemoteArchiveSynchronizationThreads
        << m_customReleaseListUrlAdaptor
        << m_targetUpdateInformationAdaptor
        << m_installedUpdateInformationAdaptor
        << m_maxVirtualCameraArchiveSynchronizationThreads
        << m_watermarkSettingsAdaptor
        << m_sessionTimeoutLimitSecondsAdaptor
        << m_sessionsLimitAdaptor
        << m_sessionsLimitPerUserAdaptor
        << m_remoteSessionUpdateAdaptor
        << m_remoteSessionTimeoutAdaptor
        << m_defaultVideoCodecAdaptor
        << m_defaultExportVideoCodecAdaptor
        << m_downloaderPeersAdaptor
        << m_clientUpdateSettingsAdaptor
        << m_lowQualityScreenVideoCodecAdaptor
        << m_licenseServerUrlAdaptor
        << m_channelPartnerServerUrlAdaptor
        << m_resourceFileUriAdaptor
        << m_maxHttpTranscodingSessions
        << m_maxEventLogRecordsAdaptor
        << m_forceLiveCacheForPrimaryStreamAdaptor
        << m_metadataStorageChangePolicyAdaptor
        << m_maxRtpRetryCount
        << m_targetPersistentUpdateStorageAdaptor
        << m_installedPersistentUpdateStorageAdaptor
        << m_specificFeaturesAdaptor
        << m_cloudNotificationsLanguageAdaptor
        << m_additionalLocalFsTypesAdaptor
        << m_keepIoPortStateIntactOnInitializationAdaptor
        << m_mediaBufferSizeKbAdaptor
        << m_mediaBufferSizeKbForAudioOnlyDeviceAdaptor
        << m_forceAnalyticsDbStoragePermissionsAdaptor
        << m_useStorageEncryptionAdaptor
        << m_currentStorageEncryptionKeyAdaptor
        << m_showServersInTreeForNonAdminsAdaptor
        << m_supportedOriginsAdaptor
        << m_frameOptionsHeaderAdaptor
        << m_showMouseTimelinePreviewAdaptor
        << m_ldapAdaptor
        << m_exposeServerEndpointsAdaptor
        << m_cloudPollingIntervalAdaptor
    ;

    return result;
}

QString SystemSettings::disabledVendors() const
{
    return m_disabledVendorsAdaptor->value();
}

QSet<QString> SystemSettings::disabledVendorsSet() const
{
    return parseDisabledVendors(disabledVendors());
}

void SystemSettings::setDisabledVendors(QString disabledVendors)
{
    m_disabledVendorsAdaptor->setValue(disabledVendors);
}

bool SystemSettings::isCameraSettingsOptimizationEnabled() const
{
    return m_cameraSettingsOptimizationAdaptor->value();
}

void SystemSettings::setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled)
{
    m_cameraSettingsOptimizationAdaptor->setValue(cameraSettingsOptimizationEnabled);
}

bool SystemSettings::isAutoUpdateThumbnailsEnabled() const
{
    return m_autoUpdateThumbnailsAdaptor->value();
}

void SystemSettings::setAutoUpdateThumbnailsEnabled(bool value)
{
    m_autoUpdateThumbnailsAdaptor->setValue(value);
}

int SystemSettings::maxSceneItemsOverride() const
{
    return m_maxSceneItemsAdaptor->value();
}

void SystemSettings::setMaxSceneItemsOverride(int value)
{
    m_maxSceneItemsAdaptor->setValue(value);
}

bool SystemSettings::isUseTextEmailFormat() const
{
    return m_useTextEmailFormatAdaptor->value();
}

void SystemSettings::setUseTextEmailFormat(bool value)
{
    m_useTextEmailFormatAdaptor->setValue(value);
}

bool SystemSettings::isUseWindowsEmailLineFeed() const
{
    return m_useWindowsEmailLineFeedAdaptor->value();
}

void SystemSettings::setUseWindowsEmailLineFeed(bool value)
{
    m_useWindowsEmailLineFeedAdaptor->setValue(value);
}

bool SystemSettings::isAuditTrailEnabled() const
{
    return m_auditTrailEnabledAdaptor->value();
}

void SystemSettings::setAuditTrailEnabled(bool value)
{
    m_auditTrailEnabledAdaptor->setValue(value);
}

std::chrono::days SystemSettings::auditTrailPeriodDays() const
{
    return std::chrono::days(m_auditTrailPeriodDaysAdaptor->value());
}

std::chrono::days SystemSettings::eventLogPeriodDays() const
{
    return std::chrono::days(m_eventLogPeriodDaysAdaptor->value());
}

bool SystemSettings::isTrafficEncryptionForced() const
{
    return m_trafficEncryptionForcedAdaptor->value();
}

void SystemSettings::setTrafficEncryptionForced(bool value)
{
    m_trafficEncryptionForcedAdaptor->setValue(value);
}

bool SystemSettings::isVideoTrafficEncryptionForced() const
{
    return m_videoTrafficEncryptionForcedAdaptor->value();
}

void SystemSettings::setVideoTrafficEncryptionForced(bool value)
{
    m_videoTrafficEncryptionForcedAdaptor->setValue(value);
}

bool SystemSettings::exposeDeviceCredentials() const
{
    return m_exposeDeviceCredentialsAdaptor->value();
}

void SystemSettings::setExposeDeviceCredentials(bool value)
{
    m_exposeDeviceCredentialsAdaptor->setValue(value);
}

bool SystemSettings::isAutoDiscoveryEnabled() const
{
    return m_autoDiscoveryEnabledAdaptor->value();
}

void SystemSettings::setAutoDiscoveryEnabled(bool enabled)
{
    m_autoDiscoveryEnabledAdaptor->setValue(enabled);
}

bool SystemSettings::isAutoDiscoveryResponseEnabled() const
{
    return m_autoDiscoveryResponseEnabledAdaptor->value();
}

void SystemSettings::setAutoDiscoveryResponseEnabled(bool enabled)
{
    m_autoDiscoveryResponseEnabledAdaptor->setValue(enabled);
}

void SystemSettings::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    if (!m_admin || resource != m_admin)
        return;

    NX_MUTEX_LOCKER locker( &m_mutex );
    m_admin.reset();

    for (auto adaptor: m_allAdaptors)
        adaptor->setResource(QnResourcePtr());
}

nx::vms::api::LdapSettings SystemSettings::ldap() const
{
    return m_ldapAdaptor->value();
}

void SystemSettings::setLdap(nx::vms::api::LdapSettings settings)
{
    m_ldapAdaptor->setValue(std::move(settings));
}

QnEmailSettings SystemSettings::emailSettings() const
{
    return m_emailSettingsAdaptor->value();
}

void SystemSettings::setEmailSettings(const QnEmailSettings& emailSettings)
{
    m_emailSettingsAdaptor->setValue(static_cast<nx::vms::api::EmailSettings>(emailSettings));
}

void SystemSettings::synchronizeNow()
{
    for (auto adaptor: m_allAdaptors)
        adaptor->saveToResource();

    NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_admin)
        return;

    m_context->resourcePropertyDictionary()->saveParamsAsync(m_admin->getId());
}

bool SystemSettings::resynchronizeNowSync(nx::utils::MoveOnlyFunc<
    bool(const QString& paramName, const QString& paramValue)> filter)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        NX_ASSERT(m_admin, "Invalid sync state");
        if (!m_admin)
            return false;
        m_context->resourcePropertyDictionary()->markAllParamsDirty(m_admin->getId(), std::move(filter));
    }
    return synchronizeNowSync();
}

bool SystemSettings::synchronizeNowSync()
{
    for (auto adaptor : m_allAdaptors)
        adaptor->saveToResource();

    NX_MUTEX_LOCKER locker(&m_mutex);
    NX_ASSERT(m_admin, "Invalid sync state");
    if (!m_admin)
        return false;
    return m_context->resourcePropertyDictionary()->saveParams(m_admin->getId());
}

bool SystemSettings::takeFromSettings(QSettings* settings, const QnResourcePtr& mediaServer)
{
    bool changed = false;
    for (const auto adapter: m_allAdaptors)
        changed |= adapter->takeFromSettings(settings, "system.");

    // TODO: These are probably deprecated and should be removed at some point in the future.
    changed |= m_statisticsAllowedAdaptor->takeFromSettings(settings, "");
    changed |= m_statisticsReportTimeCycleAdaptor->takeFromSettings(settings, "");
    changed |= m_statisticsReportUpdateDelayAdaptor->takeFromSettings(settings, "");
    changed |= m_statisticsReportServerApiAdaptor->takeFromSettings(settings, "");
    changed |= m_clientStatisticsSettingsUrlAdaptor->takeFromSettings(settings, "");
    changed |= m_cloudSystemIdAdaptor->takeFromSettings(settings, "");
    changed |= m_cloudAuthKeyAdaptor->takeFromSettings(settings, "");

    if (!changed)
        return false;

    if (!synchronizeNowSync())
        return false;

    settings->sync();
    return true;
}

bool SystemSettings::isUpdateNotificationsEnabled() const
{
    return m_updateNotificationsEnabledAdaptor->value();
}

void SystemSettings::setUpdateNotificationsEnabled(bool updateNotificationsEnabled)
{
    m_updateNotificationsEnabledAdaptor->setValue(updateNotificationsEnabled);
}

nx::vms::api::BackupSettings SystemSettings::backupSettings() const
{
    return m_backupSettingsAdaptor->value();
}

void SystemSettings::setBackupSettings(const nx::vms::api::BackupSettings& value)
{
    m_backupSettingsAdaptor->setValue(value);
}

bool SystemSettings::isStatisticsAllowed() const
{
    return m_statisticsAllowedAdaptor->value();
}

void SystemSettings::setStatisticsAllowed(bool value)
{
    m_statisticsAllowedAdaptor->setValue(value);
}

QDateTime SystemSettings::statisticsReportLastTime() const
{
    return QDateTime::fromString(m_statisticsReportLastTimeAdaptor->value(), Qt::ISODate);
}

void SystemSettings::setStatisticsReportLastTime(const QDateTime& value)
{
    m_statisticsReportLastTimeAdaptor->setValue(value.toString(Qt::ISODate));
}

QString SystemSettings::statisticsReportLastVersion() const
{
    return m_statisticsReportLastVersionAdaptor->value();
}

void SystemSettings::setStatisticsReportLastVersion(const QString& value)
{
    m_statisticsReportLastVersionAdaptor->setValue(value);
}

int SystemSettings::statisticsReportLastNumber() const
{
    return m_statisticsReportLastNumberAdaptor->value();
}

void SystemSettings::setStatisticsReportLastNumber(int value)
{
    m_statisticsReportLastNumberAdaptor->setValue(value);
}

QString SystemSettings::statisticsReportTimeCycle() const
{
    return m_statisticsReportTimeCycleAdaptor->value();
}

void SystemSettings::setStatisticsReportTimeCycle(const QString& value)
{
    m_statisticsReportTimeCycleAdaptor->setValue(value);
}

QString SystemSettings::statisticsReportUpdateDelay() const
{
    return m_statisticsReportUpdateDelayAdaptor->value();
}

void SystemSettings::setStatisticsReportUpdateDelay(const QString& value)
{
    m_statisticsReportUpdateDelayAdaptor->setValue(value);
}

bool SystemSettings::isUpnpPortMappingEnabled() const
{
    return m_upnpPortMappingEnabledAdaptor->value();
}

void SystemSettings::setUpnpPortMappingEnabled(bool value)
{
    m_upnpPortMappingEnabledAdaptor->setValue(value);
}

QnUuid SystemSettings::localSystemId() const
{
    NX_VERBOSE(this, "Providing local System ID %1", m_localSystemIdAdaptor->value());
    return m_localSystemIdAdaptor->value();
}

void SystemSettings::setLocalSystemId(const QnUuid& value)
{
    if (const auto oldValue = m_localSystemIdAdaptor->value(); oldValue != value)
    {
        NX_DEBUG(this, "Changing local System ID from %1 to %2", oldValue, value);
        m_localSystemIdAdaptor->setValue(value);
    }
}

QnUuid SystemSettings::lastMergeMasterId() const
{
    return QnUuid(m_lastMergeMasterIdAdaptor->value());
}

void SystemSettings::setLastMergeMasterId(const QnUuid& value)
{
    m_lastMergeMasterIdAdaptor->setValue(value.toString());
}

QnUuid SystemSettings::lastMergeSlaveId() const
{
    return QnUuid(m_lastMergeSlaveIdAdaptor->value());
}

void SystemSettings::setLastMergeSlaveId(const QnUuid& value)
{
    m_lastMergeSlaveIdAdaptor->setValue(value.toString());
}

nx::utils::Url SystemSettings::clientStatisticsSettingsUrl() const
{
    return nx::utils::Url::fromUserInput(m_clientStatisticsSettingsUrlAdaptor->value().trimmed());
}

QString SystemSettings::statisticsReportServerApi() const
{
    return m_statisticsReportServerApiAdaptor->value();
}

void SystemSettings::setStatisticsReportServerApi(const QString& value)
{
    m_statisticsReportServerApiAdaptor->setValue(value);
}

int SystemSettings::maxEventLogRecords() const
{
    return m_maxEventLogRecordsAdaptor->value();
}

void SystemSettings::setMaxEventLogRecords(int value)
{
    m_maxEventLogRecordsAdaptor->setValue(value);
}

std::chrono::seconds SystemSettings::aliveUpdateInterval() const
{
    return std::chrono::seconds(m_ec2AliveUpdateIntervalAdaptor->value());
}

void SystemSettings::setAliveUpdateInterval(std::chrono::seconds newInterval) const
{
    m_ec2AliveUpdateIntervalAdaptor->setValue(newInterval.count());
}

bool SystemSettings::isTimeSynchronizationEnabled() const
{
    return m_timeSynchronizationEnabledAdaptor->value();
}

void SystemSettings::setTimeSynchronizationEnabled(bool value)
{
    m_timeSynchronizationEnabledAdaptor->setValue(value);
}

QnUuid SystemSettings::primaryTimeServer() const
{
    return m_primaryTimeServerAdaptor->value();
}

void SystemSettings::setPrimaryTimeServer(const QnUuid& value)
{
    m_primaryTimeServerAdaptor->setValue(value);
}

std::chrono::milliseconds SystemSettings::maxDifferenceBetweenSynchronizedAndInternetTime() const
{
    return std::chrono::milliseconds(
        m_maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor->value());
}

std::chrono::milliseconds SystemSettings::maxDifferenceBetweenSynchronizedAndLocalTime() const
{
    return std::chrono::milliseconds(
        m_maxDifferenceBetweenSynchronizedAndLocalTimeAdaptor->value());
}

std::chrono::milliseconds SystemSettings::osTimeChangeCheckPeriod() const
{
    return std::chrono::milliseconds(m_osTimeChangeCheckPeriodAdaptor->value());
}

void SystemSettings::setOsTimeChangeCheckPeriod(std::chrono::milliseconds value)
{
    m_osTimeChangeCheckPeriodAdaptor->setValue(value.count());
}

std::chrono::milliseconds SystemSettings::syncTimeExchangePeriod() const
{
    return std::chrono::milliseconds(m_syncTimeExchangePeriodAdaptor->value());
}

void SystemSettings::setSyncTimeExchangePeriod(std::chrono::milliseconds value)
{
    m_syncTimeExchangePeriodAdaptor->setValue(value.count());
}

std::chrono::milliseconds SystemSettings::syncTimeEpsilon() const
{
    return std::chrono::milliseconds(m_syncTimeEpsilonAdaptor->value());
}

void SystemSettings::setSyncTimeEpsilon(std::chrono::milliseconds value)
{
    m_syncTimeEpsilonAdaptor->setValue(value.count());
}

QString SystemSettings::cloudAccountName() const
{
    return m_cloudAccountNameAdaptor->value();
}

void SystemSettings::setCloudAccountName(const QString& value)
{
    m_cloudAccountNameAdaptor->setValue(value);
}

QnUuid SystemSettings::organizationId() const
{
    return m_organizationIdAdaptor->value();
}

void SystemSettings::setOrganizationId(const QnUuid& value)
{
    m_organizationIdAdaptor->setValue(value);
}

QString SystemSettings::cloudSystemId() const
{
    return m_cloudSystemIdAdaptor->value();
}

void SystemSettings::setCloudSystemId(const QString& value)
{
    m_cloudSystemIdAdaptor->setValue(value);
}

QString SystemSettings::cloudAuthKey() const
{
    return m_cloudAuthKeyAdaptor->value();
}

void SystemSettings::setCloudAuthKey(const QString& value)
{
    m_cloudAuthKeyAdaptor->setValue(value);
}

QString SystemSettings::systemName() const
{
    return m_systemNameAdaptor->value();
}

void SystemSettings::setSystemName(const QString& value)
{
    m_systemNameAdaptor->setValue(value);
}

void SystemSettings::resetCloudParams()
{
    setCloudAccountName(QString());
    setCloudSystemId(QString());
    setCloudAuthKey(QString());
}

QString SystemSettings::cloudHost() const
{
    return m_cloudHostAdaptor->value();
}

void SystemSettings::setCloudHost(const QString& value)
{
    m_cloudHostAdaptor->setValue(value);
}

bool SystemSettings::crossdomainEnabled() const
{
    return m_crossdomainEnabledAdaptor->value();
}

void SystemSettings::setCrossdomainEnabled(bool value)
{
    m_crossdomainEnabledAdaptor->setValue(value);
}

bool SystemSettings::arecontRtspEnabled() const
{
    return m_arecontRtspEnabledAdaptor->value();
}

void SystemSettings::setArecontRtspEnabled(bool newVal) const
{
    m_arecontRtspEnabledAdaptor->setValue(newVal);
}

int SystemSettings::maxRtpRetryCount() const
{
    return m_maxRtpRetryCount->value();
}

void SystemSettings::setMaxRtpRetryCount(int newVal)
{
    m_maxRtpRetryCount->setValue(newVal);
}

bool SystemSettings::sequentialFlirOnvifSearcherEnabled() const
{
    return m_sequentialFlirOnvifSearcherEnabledAdaptor->value();
}

void SystemSettings::setSequentialFlirOnvifSearcherEnabled(bool newVal)
{
    m_sequentialFlirOnvifSearcherEnabledAdaptor->setValue(newVal);
}

int SystemSettings::rtpFrameTimeoutMs() const
{
    return m_rtpFrameTimeoutMs->value();
}

void SystemSettings::setRtpFrameTimeoutMs(int newValue)
{
    m_rtpFrameTimeoutMs->setValue(newValue);
}

std::chrono::seconds SystemSettings::maxRtspConnectDuration() const
{
    return std::chrono::seconds(m_maxRtspConnectDuration->value());
}

void SystemSettings::setMaxRtspConnectDuration(std::chrono::seconds newValue)
{
    m_maxRtspConnectDuration->setValue(newValue.count());
}

int SystemSettings::maxP2pQueueSizeBytes() const
{
    return m_maxP2pQueueSizeBytes->value();
}

qint64 SystemSettings::maxP2pQueueSizeForAllClientsBytes() const
{
    return m_maxP2pQueueSizeForAllClientsBytes->value();
}

int SystemSettings::maxRecorderQueueSizeBytes() const
{
    return m_maxRecorderQueueSizeBytes->value();
}

int SystemSettings::maxHttpTranscodingSessions() const
{
    return m_maxHttpTranscodingSessions->value();
}

int SystemSettings::maxRecorderQueueSizePackets() const
{
    return m_maxRecorderQueueSizePackets->value();
}

bool SystemSettings::isEdgeRecordingEnabled() const
{
    return m_edgeRecordingEnabledAdaptor->value();
}

void SystemSettings::setEdgeRecordingEnabled(bool enabled)
{
    m_edgeRecordingEnabledAdaptor->setValue(enabled);
}

nx::utils::Url SystemSettings::customReleaseListUrl() const
{
    return m_customReleaseListUrlAdaptor->value();
}

void SystemSettings::setCustomReleaseListUrl(const nx::utils::Url& url)
{
    m_customReleaseListUrlAdaptor->setValue(url);
}

QByteArray SystemSettings::targetUpdateInformation() const
{
    return m_targetUpdateInformationAdaptor->value();
}

bool SystemSettings::isWebSocketEnabled() const
{
    return m_webSocketEnabledAdaptor->value();
}

void SystemSettings::setWebSocketEnabled(bool enabled)
{
    m_webSocketEnabledAdaptor->setValue(enabled);
}

void SystemSettings::setTargetUpdateInformation(const QByteArray& updateInformation)
{
    m_targetUpdateInformationAdaptor->setValue(updateInformation);
}

nx::vms::common::update::PersistentUpdateStorage SystemSettings::targetPersistentUpdateStorage() const
{
    return m_targetPersistentUpdateStorageAdaptor->value();
}

void SystemSettings::setTargetPersistentUpdateStorage(
    const nx::vms::common::update::PersistentUpdateStorage& data)
{
    m_targetPersistentUpdateStorageAdaptor->setValue(data);
}

nx::vms::common::update::PersistentUpdateStorage SystemSettings::installedPersistentUpdateStorage() const
{
    return m_installedPersistentUpdateStorageAdaptor->value();
}

void SystemSettings::setInstalledPersistentUpdateStorage(
    const nx::vms::common::update::PersistentUpdateStorage& data)
{
    m_installedPersistentUpdateStorageAdaptor->setValue(data);
}

QByteArray SystemSettings::installedUpdateInformation() const
{
    return m_installedUpdateInformationAdaptor->value();
}

void SystemSettings::setInstalledUpdateInformation(const QByteArray& updateInformation)
{
    m_installedUpdateInformationAdaptor->setValue(updateInformation);
}

FileToPeerList SystemSettings::downloaderPeers() const
{
    return m_downloaderPeersAdaptor->value();
}

void SystemSettings::setdDownloaderPeers(const FileToPeerList& downloaderPeers)
{
    m_downloaderPeersAdaptor->setValue(downloaderPeers);
}

api::ClientUpdateSettings SystemSettings::clientUpdateSettings() const
{
    return m_clientUpdateSettingsAdaptor->value();
}

void SystemSettings::setClientUpdateSettings(const api::ClientUpdateSettings& settings)
{
    m_clientUpdateSettingsAdaptor->setValue(settings);
}

int SystemSettings::maxRemoteArchiveSynchronizationThreads() const
{
    return m_maxRemoteArchiveSynchronizationThreads->value();
}

void SystemSettings::setMaxRemoteArchiveSynchronizationThreads(int newValue)
{
    m_maxRemoteArchiveSynchronizationThreads->setValue(newValue);
}

int SystemSettings::maxVirtualCameraArchiveSynchronizationThreads() const
{
    return m_maxVirtualCameraArchiveSynchronizationThreads->value();
}

void SystemSettings::setMaxVirtualCameraArchiveSynchronizationThreads(int newValue)
{
    m_maxVirtualCameraArchiveSynchronizationThreads->setValue(newValue);
}

std::chrono::seconds SystemSettings::proxyConnectTimeout() const
{
    return std::chrono::seconds(m_proxyConnectTimeoutAdaptor->value());
}

bool SystemSettings::cloudConnectUdpHolePunchingEnabled() const
{
    return m_cloudConnectUdpHolePunchingEnabledAdaptor->value();
}

bool SystemSettings::cloudConnectRelayingEnabled() const
{
    return m_cloudConnectRelayingEnabledAdaptor->value();
}

bool SystemSettings::cloudConnectRelayingOverSslForced() const
{
    return m_cloudConnectRelayingOverSslForcedAdaptor->value();
}

nx::vms::api::WatermarkSettings SystemSettings::watermarkSettings() const
{
    return m_watermarkSettingsAdaptor->value();
}

void SystemSettings::setWatermarkSettings(const nx::vms::api::WatermarkSettings& settings) const
{
    m_watermarkSettingsAdaptor->setValue(settings);
}

std::optional<std::chrono::seconds> SystemSettings::sessionTimeoutLimit() const
{
    int seconds = m_sessionTimeoutLimitSecondsAdaptor->value();
    return seconds > 0
        ? std::optional<std::chrono::seconds>(seconds)
        : std::nullopt;
}

void SystemSettings::setSessionTimeoutLimit(std::optional<std::chrono::seconds> value)
{
    int seconds = value ? value->count() : 0;
    m_sessionTimeoutLimitSecondsAdaptor->setValue(seconds);
}

int SystemSettings::sessionsLimit() const
{
    return m_sessionsLimitAdaptor->value();
}

void SystemSettings::setSessionsLimit(int value)
{
    m_sessionsLimitAdaptor->setValue(value);
}

int SystemSettings::sessionsLimitPerUser() const
{
    return m_sessionsLimitPerUserAdaptor->value();
}

void SystemSettings::setSessionsLimitPerUser(int value)
{
    m_sessionsLimitPerUserAdaptor->setValue(value);
}

std::chrono::seconds SystemSettings::remoteSessionUpdate() const
{
    return std::chrono::seconds(m_remoteSessionUpdateAdaptor->value());
}

void SystemSettings::setRemoteSessionUpdate(std::chrono::seconds value)
{
    m_remoteSessionUpdateAdaptor->setValue(value.count());
}

std::chrono::seconds SystemSettings::remoteSessionTimeout() const
{
    return std::chrono::seconds(m_remoteSessionTimeoutAdaptor->value());
}

void SystemSettings::setRemoteSessionTimeout(std::chrono::seconds value)
{
    m_remoteSessionTimeoutAdaptor->setValue(value.count());
}

QString SystemSettings::defaultVideoCodec() const
{
    return m_defaultVideoCodecAdaptor->value();
}

void SystemSettings::setDefaultVideoCodec(const QString& value)
{
    m_defaultVideoCodecAdaptor->setValue(value);
}

QString SystemSettings::defaultExportVideoCodec() const
{
    return m_defaultExportVideoCodecAdaptor->value();
}

void SystemSettings::setDefaultExportVideoCodec(const QString& value)
{
    m_defaultExportVideoCodecAdaptor->setValue(value);
}

QString SystemSettings::lowQualityScreenVideoCodec() const
{
    return m_lowQualityScreenVideoCodecAdaptor->value();
}

void SystemSettings::setLicenseServerUrl(const QString& value)
{
    m_licenseServerUrlAdaptor->setValue(value);
}

QString SystemSettings::licenseServerUrl() const
{
    return m_licenseServerUrlAdaptor->value();
}

void SystemSettings::setChannelPartnerServerUrl(const nx::utils::Url& value)
{
    m_channelPartnerServerUrlAdaptor->setValue(value);
}

nx::utils::Url SystemSettings::channelPartnerServerUrl() const
{
    return m_channelPartnerServerUrlAdaptor->value();
}

nx::utils::Url SystemSettings::resourceFileUri() const
{
    return m_resourceFileUriAdaptor->value();
}

QString SystemSettings::cloudNotificationsLanguage() const
{
    return m_cloudNotificationsLanguageAdaptor->value();
}

void SystemSettings::setCloudNotificationsLanguage(const QString& value)
{
    m_cloudNotificationsLanguageAdaptor->setValue(value);
}

bool SystemSettings::keepIoPortStateIntactOnInitialization() const
{
    return m_keepIoPortStateIntactOnInitializationAdaptor->value();
}

void SystemSettings::setKeepIoPortStateIntactOnInitialization(bool value)
{
    m_keepIoPortStateIntactOnInitializationAdaptor->setValue(value);
}

int SystemSettings::mediaBufferSizeKb() const
{
    return m_mediaBufferSizeKbAdaptor->value();
}

void SystemSettings::setMediaBufferSizeKb(int value)
{
    m_mediaBufferSizeKbAdaptor->setValue(value);
}

int SystemSettings::mediaBufferSizeForAudioOnlyDeviceKb() const
{
    return m_mediaBufferSizeKbForAudioOnlyDeviceAdaptor->value();
}

void SystemSettings::setMediaBufferSizeForAudioOnlyDeviceKb(int value)
{
    m_mediaBufferSizeKbForAudioOnlyDeviceAdaptor->setValue(value);
}

bool SystemSettings::forceAnalyticsDbStoragePermissions() const
{
    return m_forceAnalyticsDbStoragePermissionsAdaptor->value();
}

std::chrono::milliseconds SystemSettings::checkVideoStreamPeriod() const
{
    return std::chrono::milliseconds(m_checkVideoStreamPeriodMsAdaptor->value());
}

void SystemSettings::setCheckVideoStreamPeriod(std::chrono::milliseconds value)
{
    m_checkVideoStreamPeriodMsAdaptor->setValue(value.count());
}

bool SystemSettings::useStorageEncryption() const
{
    return m_useStorageEncryptionAdaptor->value();
}

void SystemSettings::setUseStorageEncryption(bool value)
{
    m_useStorageEncryptionAdaptor->setValue(value);
}

QByteArray SystemSettings::currentStorageEncryptionKey() const
{
    return m_currentStorageEncryptionKeyAdaptor->value();
}

void SystemSettings::setCurrentStorageEncryptionKey(const QByteArray& value)
{
    m_currentStorageEncryptionKeyAdaptor->setValue(value);
}

bool SystemSettings::showServersInTreeForNonAdmins() const
{
    return m_showServersInTreeForNonAdminsAdaptor->value();
}

void SystemSettings::setShowServersInTreeForNonAdmins(bool value)
{
    m_showServersInTreeForNonAdminsAdaptor->setValue(value);
}

QString SystemSettings::additionalLocalFsTypes() const
{
    return m_additionalLocalFsTypesAdaptor->value();
}

void SystemSettings::setLowQualityScreenVideoCodec(const QString& value)
{
    m_lowQualityScreenVideoCodecAdaptor->setValue(value);
}

QString SystemSettings::forceLiveCacheForPrimaryStream() const
{
    return m_forceLiveCacheForPrimaryStreamAdaptor->value();
}

void SystemSettings::setForceLiveCacheForPrimaryStream(const QString& value)
{
    m_forceLiveCacheForPrimaryStreamAdaptor->setValue(value);
}

const QList<QnAbstractResourcePropertyAdaptor*>& SystemSettings::allSettings() const
{
    return m_allAdaptors;
}

QList<const QnAbstractResourcePropertyAdaptor*> SystemSettings::allDefaultSettings() const
{
    QList<const QnAbstractResourcePropertyAdaptor*> result;
    for (const auto a: m_allAdaptors)
    {
        if (a->isDefault())
            result.push_back(a);
    }

    return result;
}

bool SystemSettings::isGlobalSetting(const nx::vms::api::ResourceParamWithRefData& param)
{
    return QnUserResource::kAdminGuid == param.resourceId;
}

nx::vms::api::MetadataStorageChangePolicy SystemSettings::metadataStorageChangePolicy() const
{
    return m_metadataStorageChangePolicyAdaptor->value();
}

void SystemSettings::setMetadataStorageChangePolicy(nx::vms::api::MetadataStorageChangePolicy value)
{
    m_metadataStorageChangePolicyAdaptor->setValue(value);
}

QString SystemSettings::supportedOrigins() const
{
    return m_supportedOriginsAdaptor->value();
}
void SystemSettings::setSupportedOrigins(const QString& value)
{
    m_supportedOriginsAdaptor->setValue(value);
}

QString SystemSettings::frameOptionsHeader() const
{
    return m_frameOptionsHeaderAdaptor->value();
}
void SystemSettings::setFrameOptionsHeader(const QString& value)
{
    m_frameOptionsHeaderAdaptor->setValue(value);
}

bool SystemSettings::exposeServerEndpoints() const
{
    return m_exposeServerEndpointsAdaptor->value();
}

void SystemSettings::setExposeServerEndpoints(bool value)
{
    m_exposeServerEndpointsAdaptor->setValue(value);
}

bool SystemSettings::showMouseTimelinePreview() const
{
    return m_showMouseTimelinePreviewAdaptor->value();
}

void SystemSettings::setShowMouseTimelinePreview(bool value)
{
    m_showMouseTimelinePreviewAdaptor->setValue(value);
}

bool SystemSettings::system2faEnabled() const
{
    return m_system2faEnabledAdaptor->value();
}

void SystemSettings::setSystem2faEnabled(bool value)
{
    m_system2faEnabledAdaptor->setValue(value);
}

std::vector<QnUuid> SystemSettings::masterCloudSyncList() const
{
    std::vector<QnUuid> result;
    for (const auto& value: m_masterCloudSyncAdaptor->value().split(";", Qt::SkipEmptyParts))
        result.push_back(QnUuid(value));
    return result;
}

void SystemSettings::setMasterCloudSyncList(const std::vector<QnUuid>& idList) const
{
    QStringList result;
    for (const auto& id: idList)
        result << id.toString();
    m_masterCloudSyncAdaptor->setValue(result.join(";"));
}

std::map<QString, int> SystemSettings::specificFeatures() const
{
    return m_specificFeaturesAdaptor->value();
}

void SystemSettings::setSpecificFeatures(std::map<QString, int> value)
{
    m_specificFeaturesAdaptor->setValue(value);
}

bool SystemSettings::useHttpsOnlyForCameras() const
{
    return m_useHttpsOnlyForCamerasAdaptor->value();
}

void SystemSettings::setUseHttpsOnlyForCameras(bool value)
{
    m_useHttpsOnlyForCamerasAdaptor->setValue(value);
}

bool SystemSettings::securityForPowerUsers() const
{
    return m_securityForPowerUsersAdaptor->value();
}

void SystemSettings::setSecurityForPowerUsers(bool value)
{
    m_securityForPowerUsersAdaptor->setValue(value);
}

bool SystemSettings::isInsecureDeprecatedApiEnabled() const
{
    return m_insecureDeprecatedApiEnabledAdaptor->value();
}

void SystemSettings::enableInsecureDeprecatedApi(bool value)
{
    m_insecureDeprecatedApiEnabledAdaptor->setValue(value);
}

bool SystemSettings::isInsecureDeprecatedApiInUseEnabled() const
{
    return m_insecureDeprecatedApiInUseEnabledAdaptor->value();
}

void SystemSettings::enableInsecureDeprecatedApiInUse(bool value)
{
    m_insecureDeprecatedApiInUseEnabledAdaptor->setValue(value);
}

std::chrono::seconds SystemSettings::cloudPollingInterval() const
{
    return std::chrono::seconds(m_cloudPollingIntervalAdaptor->value());
}

void SystemSettings::setCloudPollingInterval(std::chrono::seconds period)
{
    m_cloudPollingIntervalAdaptor->setValue(period.count());
}

void SystemSettings::update(const vms::api::SystemSettings& value)
{
    m_cloudAccountNameAdaptor->setValue(value.cloudAccountName);
    m_cloudSystemIdAdaptor->setValue(value.cloudSystemID);
    m_defaultExportVideoCodecAdaptor->setValue(value.defaultExportVideoCodec);
    m_localSystemIdAdaptor->setValue(value.localSystemId);
    m_systemNameAdaptor->setValue(value.systemName);
    m_watermarkSettingsAdaptor->setValue(value.watermarkSettings);
    m_webSocketEnabledAdaptor->setValue(value.webSocketEnabled);
    m_autoDiscoveryEnabledAdaptor->setValue(value.autoDiscoveryEnabled);
    m_cameraSettingsOptimizationAdaptor->setValue(value.cameraSettingsOptimization);
    m_statisticsAllowedAdaptor->setValue(value.statisticsAllowed);
    m_cloudNotificationsLanguageAdaptor->setValue(value.cloudNotificationsLanguage);
    m_auditTrailEnabledAdaptor->setValue(value.auditTrailEnabled);
    m_trafficEncryptionForcedAdaptor->setValue(value.trafficEncryptionForced);
    m_useHttpsOnlyForCamerasAdaptor->setValue(value.useHttpsOnlyForCameras);
    m_videoTrafficEncryptionForcedAdaptor->setValue(value.videoTrafficEncryptionForced);
    m_sessionTimeoutLimitSecondsAdaptor->setValue(value.sessionLimitS.value_or(0s).count());
    m_useStorageEncryptionAdaptor->setValue(value.storageEncryption);
    m_showServersInTreeForNonAdminsAdaptor->setValue(value.showServersInTreeForNonAdmins);
    m_updateNotificationsEnabledAdaptor->setValue(value.updateNotificationsEnabled);
    m_emailSettingsAdaptor->setValue(value.emailSettings);
    m_timeSynchronizationEnabledAdaptor->setValue(value.timeSynchronizationEnabled);
    m_primaryTimeServerAdaptor->setValue(value.primaryTimeServer);
    m_customReleaseListUrlAdaptor->setValue(value.customReleaseListUrl);
    m_clientUpdateSettingsAdaptor->setValue(value.clientUpdateSettings);
    m_backupSettingsAdaptor->setValue(value.backupSettings);
    m_metadataStorageChangePolicyAdaptor->setValue(value.metadataStorageChangePolicy);
}

} // namespace nx::vms::common
