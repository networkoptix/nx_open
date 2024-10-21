// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_settings.h"

#include <chrono>

#include <api/resource_property_adaptor.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/value_cache.h>
#include <nx/vms/api/data/backup_settings.h>
#include <nx/vms/api/data/email_settings.h>
#include <nx/vms/api/data/ldap.h>
#include <nx/vms/api/data/pixelation_settings.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/api/data/watermark_settings.h>
#include <nx/vms/api/types/proxy_connection_access_policy.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_misc_manager.h>
#include <utils/email/email.h>

using namespace std::chrono;

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

std::string makeServerHeaderValue(QString value)
{
    return value
        .replace("$vmsName", nx::branding::vmsName())
        .replace("$vmsVersion", nx::build_info::vmsVersion())
        .replace("$company", nx::branding::company())
        .replace("$compatibility", QString::fromStdString(nx::network::http::compatibilityServerName()))
        .toStdString();
}

} // namespace

namespace nx::vms::common {

struct SystemSettings::Private
{
    QnResourcePropertyAdaptor<bool>* cameraSettingsOptimizationAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* autoUpdateThumbnailsAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* maxSceneItemsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* useTextEmailFormatAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* useWindowsEmailLineFeedAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* auditTrailEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* auditTrailPeriodDaysAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* eventLogPeriodDaysAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* trafficEncryptionForcedAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* videoTrafficEncryptionForcedAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* exposeDeviceCredentialsAdaptor = nullptr;

    QnResourcePropertyAdaptor<QString>* disabledVendorsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* autoDiscoveryEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* autoDiscoveryResponseEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* updateNotificationsEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* timeSynchronizationEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* synchronizeTimeWithInternetAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::Uuid> *primaryTimeServerAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* masterCloudSyncListAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::chrono::milliseconds>* maxDifferenceBetweenSynchronizedAndLocalTimeMsAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::chrono::milliseconds>* osTimeChangeCheckPeriodMsAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* syncTimeExchangePeriodAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* syncTimeEpsilonAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::api::BackupSettings>* backupSettingsAdaptor = nullptr;

    // set of statistics settings adaptors
    QnResourcePropertyAdaptor<bool>* statisticsAllowedAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* statisticsReportLastTimeAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* statisticsReportLastVersionAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* statisticsReportLastNumberAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* statisticsReportTimeCycleAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* statisticsReportUpdateDelayAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* upnpPortMappingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::Uuid>* localSystemIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* lastMergeMasterIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* lastMergeSlaveIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* statisticsReportServerApiAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* crashReportServerApiAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* clientStatisticsSettingsUrlAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::chrono::seconds>* deviceStorageInfoUpdateIntervalSAdaptor = nullptr;

    // set of email settings adaptors
    QnResourcePropertyAdaptor<nx::vms::api::EmailSettings>* emailSettingsAdaptor = nullptr;

    QnResourcePropertyAdaptor<nx::vms::api::LdapSettings>* ldapAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* ec2AliveUpdateIntervalSecAdaptor = nullptr;
    /** seconds */
    QnResourcePropertyAdaptor<int>* proxyConnectTimeoutSecAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::api::ProxyConnectionAccessPolicy>* proxyConnectionAccessPolicyAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* takeCameraOwnershipWithoutLock = nullptr;

    // set of cloud adaptors
    QnResourcePropertyAdaptor<QString>* cloudAccountNameAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::Uuid>* organizationIdAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* cloudSystemIDAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* cloudAuthKeyAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* system2faEnabledAdaptor = nullptr;

    // misc adaptors
    QnResourcePropertyAdaptor<int>* maxEventLogRecordsAdaptor = nullptr;

    QnResourcePropertyAdaptor<QString>* systemNameAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* arecontRtspEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* sequentialFlirOnvifSearcherEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* cloudHostAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* crossdomainEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* maxP2pQueueSizeBytesAdaptor = nullptr;
    QnResourcePropertyAdaptor<qint64>* maxP2pAllClientsSizeBytesAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* maxRecordQueueSizeBytesAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* maxRecordQueueSizeElementsAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* maxHttpTranscodingSessionsAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* maxRtpRetryCountAdaptor = nullptr;

    QnResourcePropertyAdaptor<std::chrono::milliseconds>* rtpTimeoutMsAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* maxRtspConnectDurationSecondsAdaptor = nullptr;

    QnResourcePropertyAdaptor<bool>* cloudConnectUdpHolePunchingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* cloudConnectRelayingEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* cloudConnectRelayingOverSslForcedAdaptor = nullptr;

    QnResourcePropertyAdaptor<bool>* enableEdgeRecordingAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* webSocketEnabledAdaptor = nullptr;

    QnResourcePropertyAdaptor<int>* maxRemoteArchiveSynchronizationThreadsAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* maxVirtualCameraArchiveSynchronizationThreadsAdaptor = nullptr;

    QnResourcePropertyAdaptor<nx::utils::Url>* customReleaseListUrlAdaptor = nullptr;
    QnResourcePropertyAdaptor<QByteArray>* targetUpdateInformationAdaptor = nullptr;
    QnResourcePropertyAdaptor<QByteArray>* installedUpdateInformationAdaptor = nullptr;
    QnResourcePropertyAdaptor<api::PersistentUpdateStorage>* targetPersistentUpdateStorageAdaptor = nullptr;
    QnResourcePropertyAdaptor<api::PersistentUpdateStorage>* installedPersistentUpdateStorageAdaptor = nullptr;
    QnResourcePropertyAdaptor<FileToPeerList>* downloaderPeersAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::api::ClientUpdateSettings>*
        clientUpdateSettingsAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::api::WatermarkSettings>* watermarkSettingsAdaptor = nullptr;

    QnResourcePropertyAdaptor<nx::vms::api::PixelationSettings>*
        pixelationSettingsAdaptor = nullptr;

    QnResourcePropertyAdaptor<std::chrono::seconds>* sessionLimitSAdaptor = nullptr;
	QnResourcePropertyAdaptor<bool>* useSessionLimitForCloudAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* sessionsLimitAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* sessionsLimitPerUserAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::chrono::seconds>* remoteSessionUpdateSAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::chrono::seconds>* remoteSessionTimeoutSAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* defaultVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* defaultExportVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* lowQualityScreenVideoCodecAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* forceLiveCacheForPrimaryStreamAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::vms::api::MetadataStorageChangePolicy>* metadataStorageChangePolicyAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::map<QString, int>>* specificFeaturesAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* licenseServerAdaptor = nullptr;
    QnResourcePropertyAdaptor<nx::utils::Url>* resourceFileUriAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* cloudNotificationsLanguageAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* additionalLocalFsTypesAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* keepIoPortStateIntactOnInitializationAdaptor= nullptr;
    QnResourcePropertyAdaptor<int>* mediaBufferSizeKbAdaptor = nullptr;
    QnResourcePropertyAdaptor<int>* mediaBufferSizeForAudioOnlyDeviceKbAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* forceAnalyticsDbStoragePermissionsAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::chrono::milliseconds>* checkVideoStreamPeriodMsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* storageEncryptionAdaptor = nullptr;
    QnResourcePropertyAdaptor<QByteArray>* currentStorageEncryptionKeyAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* showServersInTreeForNonAdminsAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* serverHeaderAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* supportedOriginsAdaptor = nullptr;
    QnResourcePropertyAdaptor<QString>* frameOptionsHeaderAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* useHttpsOnlyForCamerasAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* securityForPowerUsersAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* insecureDeprecatedApiEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* insecureDeprecatedApiInUseEnabledAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* exposeServerEndpointsAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* showMouseTimelinePreviewAdaptor = nullptr;
    QnResourcePropertyAdaptor<std::chrono::seconds>* cloudPollingIntervalSAdaptor = nullptr;
    QnResourcePropertyAdaptor<bool>* allowRegisteringIntegrationsAdaptor = nullptr;

    AdaptorList allAdaptors;
    std::unordered_set<QString> settingNames;

    mutable nx::Mutex mutex;
    QnUserResourcePtr admin;

    nx::utils::CachedValue<std::string> serverHeaderCache{
        [this] { return makeServerHeaderValue(serverHeaderAdaptor->value()); }};
};

SystemSettings::SystemSettings(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context),
    d(new Private)
{
    d->allAdaptors
        << initEmailAdaptors()
        << initStaticticsAdaptors()
        << initConnectionAdaptors()
        << initTimeSynchronizationAdaptors()
        << initCloudAdaptors()
        << initMiscAdaptors()
        ;

    for (const auto& adapter: d->allAdaptors)
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

    d->settingNames.reserve(d->allAdaptors.size());
    for (const auto& s: d->allAdaptors)
        d->settingNames.emplace(s->key());
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
        NX_MUTEX_LOCKER locker(&d->mutex);
        NX_VERBOSE(this, "System settings successfully initialized");
        d->admin = user;
        for (auto adaptor: d->allAdaptors)
            adaptor->setResource(user);
    }
    emit initialized();
}

bool SystemSettings::isInitialized() const
{
    return !d->admin.isNull();
}

SystemSettings::AdaptorList SystemSettings::initEmailAdaptors()
{
    const auto isValid =
        [](const nx::vms::api::EmailSettings& s)
        {
            return !s.useCloudServiceToSendEmail
                || s.supportAddress == nx::branding::supportAddress();
        };

    d->emailSettingsAdaptor =
        new QnJsonResourcePropertyAdaptor<nx::vms::api::EmailSettings>(
            "emailSettings",
            nx::vms::api::EmailSettings{},
            isValid,
            this,
            [] { return tr("SMTP settings"); });

    connect(
        d->emailSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::emailSettingsChanged,
        Qt::QueuedConnection);

    return {d->emailSettingsAdaptor};
}

SystemSettings::AdaptorList SystemSettings::initStaticticsAdaptors()
{
    d->statisticsAllowedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "statisticsAllowed", true, this,
        [] { return tr("Anonymous statistics report allowed"); });

    d->statisticsReportLastTimeAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportLastTime", QString(), this,
        [] { return tr("Anonymous statistics report last time"); });

    d->statisticsReportLastVersionAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportLastVersion", QString(), this,
        [] { return tr("Anonymous statistics report last version"); });

    d->statisticsReportLastNumberAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "statisticsReportLastNumber", 0, this,
        [] { return tr("Anonymous statistics report last number"); });

    d->statisticsReportTimeCycleAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportTimeCycle", "30d", this,
        [] { return tr("Anonymous statistics time cycle"); });

    d->statisticsReportUpdateDelayAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportUpdateDelay", "3h", this,
        [] { return tr("Anonymous statistics report delay after update"); });

    d->statisticsReportServerApiAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "statisticsReportServerApi", QString(), this,
        [] { return tr("Anonymous Statistics Report Server URL"); });

    d->crashReportServerApiAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "crashReportServerApi", QString(), this,
        [] { return tr("Anonymous Crash Report Server API URL"); });

    d->clientStatisticsSettingsUrlAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "clientStatisticsSettingsUrl", QString(), this,
        [] { return tr("Anonymous statistics report Client settings"); });

    d->deviceStorageInfoUpdateIntervalSAdaptor =
        new QnJsonResourcePropertyAdaptor<std::chrono::seconds>(
            "deviceStorageInfoUpdateIntervalS", 7 * 24 * 3600 * 1s, this,
            [] { return tr("Device storage information update interval"); });

    connect(
        d->statisticsAllowedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::statisticsAllowedChanged,
        Qt::QueuedConnection);

    AdaptorList result;
    result
        << d->statisticsAllowedAdaptor
        << d->statisticsReportLastTimeAdaptor
        << d->statisticsReportLastVersionAdaptor
        << d->statisticsReportLastNumberAdaptor
        << d->statisticsReportTimeCycleAdaptor
        << d->statisticsReportUpdateDelayAdaptor
        << d->statisticsReportServerApiAdaptor
        << d->crashReportServerApiAdaptor
        << d->clientStatisticsSettingsUrlAdaptor
        << d->deviceStorageInfoUpdateIntervalSAdaptor;

    return result;
}

SystemSettings::AdaptorList SystemSettings::initConnectionAdaptors()
{
    AdaptorList ec2Adaptors;

    d->ec2AliveUpdateIntervalSecAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "ec2AliveUpdateIntervalSec", 60,
        [](auto v) { return v >= 1 && v <= 60 * 60; }, this,
        [] { return tr("Site alive update interval (seconds, 1s-1h)"); });
    ec2Adaptors << d->ec2AliveUpdateIntervalSecAdaptor;

    d->proxyConnectTimeoutSecAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "proxyConnectTimeoutSec", 5,
        [](auto v) { return v >= 1 && v <= 60 * 60; }, this,
        [] { return tr("Proxy connection timeout (seconds, 1s-1h)"); });
    ec2Adaptors << d->proxyConnectTimeoutSecAdaptor;

    d->proxyConnectionAccessPolicyAdaptor =
        new QnReflectLexicalResourcePropertyAdaptor<nx::vms::api::ProxyConnectionAccessPolicy>(
            Names::proxyConnectionAccessPolicy, nx::vms::api::ProxyConnectionAccessPolicy::powerUsers, this,
            [] { return tr("Proxy connection access policy"); });
    ec2Adaptors << d->proxyConnectionAccessPolicyAdaptor;

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

    d->timeSynchronizationEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "timeSynchronizationEnabled", true, this,
        [] { return tr("Time synchronization enabled"); });
    timeSynchronizationAdaptors << d->timeSynchronizationEnabledAdaptor;

    d->primaryTimeServerAdaptor = new QnLexicalResourcePropertyAdaptor<nx::Uuid>(
        Names::primaryTimeServer, nx::Uuid(), this,
        [] { return tr("Primary time synchronization Server ID"); });
    timeSynchronizationAdaptors << d->primaryTimeServerAdaptor;

    d->maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "maxDifferenceBetweenSynchronizedAndInternetTime",
            duration_cast<milliseconds>(std::chrono::seconds(2)).count(),
            this,
            [] { return tr("Max difference between local and source time (milliseconds)"); });
    timeSynchronizationAdaptors << d->maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor;

    d->maxDifferenceBetweenSynchronizedAndLocalTimeMsAdaptor =
        new QnJsonResourcePropertyAdaptor<std::chrono::milliseconds>(
            "maxDifferenceBetweenSynchronizedAndLocalTimeMs", 2s, this,
            [] { return tr("Max difference between local and source time (milliseconds)"); });
    timeSynchronizationAdaptors << d->maxDifferenceBetweenSynchronizedAndLocalTimeMsAdaptor;

    d->osTimeChangeCheckPeriodMsAdaptor =
        new QnJsonResourcePropertyAdaptor<std::chrono::milliseconds>(
            "osTimeChangeCheckPeriodMs", 5s, this,
            [] { return tr("OS time change check period"); });
    timeSynchronizationAdaptors << d->osTimeChangeCheckPeriodMsAdaptor;

    d->syncTimeExchangePeriodAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "syncTimeExchangePeriod",
            duration_cast<milliseconds>(std::chrono::minutes(10)).count(),
            this,
            [] { return tr("Sync time synchronization interval for network requests"); });
    timeSynchronizationAdaptors << d->syncTimeExchangePeriodAdaptor;

    d->syncTimeEpsilonAdaptor =
        new QnLexicalResourcePropertyAdaptor<int>(
            "syncTimeEpsilon",
            duration_cast<milliseconds>(std::chrono::milliseconds(200)).count(),
            this,
            [] { return tr("Sync time epsilon. New value is not applied if time delta less than epsilon"); });
    timeSynchronizationAdaptors << d->syncTimeEpsilonAdaptor;

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
    d->cloudAccountNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudAccountName, QString(), this, [] { return tr("Cloud owner account"); });

    d->organizationIdAdaptor = new QnLexicalResourcePropertyAdaptor<nx::Uuid>(
        Names::organizationId, nx::Uuid(), this, [] { return tr("Organization Id"); });

    d->cloudSystemIDAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudSystemID, QString(), this, [] { return tr("Cloud Site ID"); });

    d->cloudAuthKeyAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudAuthKey, QString(), this, [] { return tr("Cloud authorization key"); });

    d->system2faEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::system2faEnabled, false, this, [] { return tr("Enable 2FA for the Site"); });

    d->masterCloudSyncListAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "masterCloudSyncList", QString(), this,
        [] { return tr("Semicolon-separated list of Servers designated to connect to the Cloud. "
            "Servers at the top of the list have higher priority. If the list is empty a Server "
            "for the Cloud connection is selected automatically."); });


    AdaptorList result;
    result
        << d->cloudAccountNameAdaptor
        << d->organizationIdAdaptor
        << d->cloudSystemIDAdaptor
        << d->cloudAuthKeyAdaptor
        << d->system2faEnabledAdaptor
        << d->masterCloudSyncListAdaptor;

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
        d->organizationIdAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::organizationIdChanged,
        Qt::QueuedConnection);

    connect(
        d->cloudSystemIDAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);

    connect(
        d->cloudAuthKeyAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudCredentialsChanged,
        Qt::QueuedConnection);

    return result;
}

SystemSettings::AdaptorList SystemSettings::initMiscAdaptors()
{
    using namespace std::chrono;

    d->systemNameAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::systemName, QString(), this, [] { return tr("Site name"); });

    d->localSystemIdAdaptor = new QnLexicalResourcePropertyAdaptor<nx::Uuid>(
        Names::localSystemId, nx::Uuid(), this,
        [] { return tr("Local Site ID, null means the Site is not set up yet."); });

    d->lastMergeMasterIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::lastMergeMasterId, QString(), this, [] { return tr("Last master Site merge ID"); });

    d->lastMergeSlaveIdAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::lastMergeSlaveId, QString(), this, [] { return tr("Last slave Site merge ID"); });

    d->disabledVendorsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::disabledVendors, QString(), this, [] { return tr("Disable Device vendors"); });

    d->cameraSettingsOptimizationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cameraSettingsOptimization", true, this, [] { return tr("Optimize Camera settings"); });

    d->autoUpdateThumbnailsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "autoUpdateThumbnails", true, this, [] { return tr("Thumbnails auto-update"); });

    d->maxSceneItemsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxSceneItems", 0,
        [this](auto v)
        {
            using namespace nx::vms::api;
            const auto saasManager = systemContext()->saasServiceManager();
            const auto limit = saasManager->tierLimit(SaasTierLimitName::maxDevicesPerLayout);
            return !limit.has_value() || (v > 0 && v <= *limit);
        },
        this,
        [] { return tr("Max scene items (0 means default)"); });

    d->useTextEmailFormatAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "useTextEmailFormat", false, this, [] { return tr("Send plain-text emails"); });

    d->useWindowsEmailLineFeedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "useWindowsEmailLineFeed", false, this,
        [] { return tr("Use Windows line feed in emails"); });

    d->auditTrailEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "auditTrailEnabled", true, this, [] { return tr("Enable audit trail"); });

    d->auditTrailPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "auditTrailPeriodDays", 183,
        [](auto v) { return v >= 14 && v <= 730; }, this,
        [] { return tr("Audit trail period (days, 14-730)"); });

    d->eventLogPeriodDaysAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "eventLogPeriodDays", 30, this, [] { return tr("Event log period (days)"); });

    d->trafficEncryptionForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::trafficEncryptionForced, true, this,
        [] { return tr("Enforce HTTPS (data traffic encryption)"); });

    d->videoTrafficEncryptionForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::videoTrafficEncryptionForced, false, this,
        [] { return tr("Enforce RTSPS (video traffic encryption)"); });

    d->exposeDeviceCredentialsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::exposeDeviceCredentials, true, this,
        [] { return tr("Expose device passwords stored in VMS for administrators (for web pages)"); });

    d->autoDiscoveryEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "autoDiscoveryEnabled", true, this,
        [] { return tr("Enable auto-discovery"); });

    d->autoDiscoveryResponseEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "autoDiscoveryResponseEnabled", true, this,
        [] { return tr("Enable auto-update notifications"); });

    d->updateNotificationsEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "updateNotificationsEnabled", true, this, [] { return tr("Enable update notifications"); });

     d->upnpPortMappingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "upnpPortMappingEnabled", true, this, [] { return tr("Enable UPNP port-mapping"); });

    d->backupSettingsAdaptor = new QnJsonResourcePropertyAdaptor<nx::vms::api::BackupSettings>(
        Names::backupSettings, nx::vms::api::BackupSettings(), this,
        [] { return tr("Backup settings"); });

    d->cloudHostAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudHost, QString(), this, [] { return tr("Cloud host override"); });

    d->crossdomainEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "crossdomainEnabled", false, this, [] { return tr("Enable cross-domain policy"); });

    d->arecontRtspEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "arecontRtspEnabled", false, this, [] { return tr("Enable RTSP for Arecont"); });

    d->sequentialFlirOnvifSearcherEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "sequentialFlirOnvifSearcherEnabled", false, this,
        [] { return tr("Enable sequential Flir ONVIF searcher"); });

    d->maxP2pQueueSizeBytesAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxP2pQueueSizeBytes", 1024 * 1024 * 128,
        [](auto v) { return v >= 1024 * 1024 * 32 && v <= 1024 * 1024 * 512; }, this,
        [] { return tr("Max P2P queue size (bytes, 32-512MB)"); });

    d->maxP2pAllClientsSizeBytesAdaptor = new QnLexicalResourcePropertyAdaptor<qint64>(
        "maxP2pAllClientsSizeBytes", 1024 * 1024 * 128,
        [](auto v) { return v >= 1024 * 1024 * 32 && v <= 1024 * 1024 * 512; }, this,
        [] { return tr("Max P2P all clients size (bytes, 32-512MB)"); });

    d->maxRecordQueueSizeBytesAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRecordQueueSizeBytes", 1024 * 1024 * 24,
        [](auto v) { return v >= 1024 * 1024 * 6 && v <= 1024 * 1024 * 96; }, this,
        [] { return tr("Max record queue size (bytes, 6-96MB)"); });

    d->maxRecordQueueSizeElementsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRecordQueueSizeElements", 1000,
        [](auto v) { return v >= 250 && v <= 4000; }, this,
        [] { return tr("Max record queue size (elements, 250-4000)"); });

    d->maxHttpTranscodingSessionsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::maxHttpTranscodingSessions, 2, this,
        [] { return tr(
            "Max amount of HTTP connections using transcoding for the Server. Chrome opens 2 "
            "connections at once, then closes the first one. "
            "We recommend setting this parameter's value to 2 or more."); });

    d->maxRtpRetryCountAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRtpRetryCount", 6, this, [] { return tr(
            "Maximum number of consecutive RTP errors before the server reconnects the RTSP "
            "session."); });

    d->rtpTimeoutMsAdaptor = new QnJsonResourcePropertyAdaptor<std::chrono::milliseconds>(
        "rtpTimeoutMs", 10s, this, [] { return tr("RTP timeout (milliseconds)"); });

    d->maxRtspConnectDurationSecondsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRtspConnectDurationSeconds", 0, this,
        [] { return tr("Max RTSP connection duration (seconds)"); });

    d->cloudConnectUdpHolePunchingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cloudConnectUdpHolePunchingEnabled", true, this,
        [] { return tr("Enable cloud-connect UDP hole-punching"); });

    d->cloudConnectRelayingEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cloudConnectRelayingEnabled", true, this,
        [] { return tr("Enable cloud-connect relays usage"); });

    d->cloudConnectRelayingOverSslForcedAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "cloudConnectRelayingOverSslForced", false, this,
        [] { return tr("Enforce SSL for cloud-connect relays"); });

    d->enableEdgeRecordingAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "enableEdgeRecording", true, this, [] { return tr("Enable recording on EDGE"); });

    d->webSocketEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::webSocketEnabled, true, this, [] { return tr("Enable WebSocket for P2P"); });

    d->maxRemoteArchiveSynchronizationThreadsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxRemoteArchiveSynchronizationThreads", -1,
        [](auto v) { return v >= -1 && v <= 32; }, this,
        [] { return tr("Max thread count for remote archive synchronization (<=0 - auto, max 32)"); });

    d->customReleaseListUrlAdaptor = new QnLexicalResourcePropertyAdaptor<utils::Url>(
        "customReleaseListUrl", nx::utils::Url(), this,
        [] { return tr("Update releases.json file URL"); });

    d->targetUpdateInformationAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        "targetUpdateInformation", QByteArray(), this,
        [] { return tr("Target update information"); });

    d->installedUpdateInformationAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        "installedUpdateInformation", QByteArray(), this,
        [] { return tr("Installed update information"); });

    d->downloaderPeersAdaptor = new QnJsonResourcePropertyAdaptor<FileToPeerList>(
        "downloaderPeers", FileToPeerList(), this, [] { return tr("Downloader peers for files"); });

    d->clientUpdateSettingsAdaptor = new QnJsonResourcePropertyAdaptor<api::ClientUpdateSettings>(
        "clientUpdateSettings", api::ClientUpdateSettings{}, this,
        [] { return tr("Client update settings"); });

    d->maxVirtualCameraArchiveSynchronizationThreadsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxVirtualCameraArchiveSynchronizationThreads", -1, this,
        [] { return tr("Thread count limit for camera archive synchronization"); });

    d->watermarkSettingsAdaptor = new QnJsonResourcePropertyAdaptor<nx::vms::api::WatermarkSettings>(
        "watermarkSettings", nx::vms::api::WatermarkSettings(), this,
        [] { return tr("Watermark settings"); });

    d->pixelationSettingsAdaptor =
        new QnJsonResourcePropertyAdaptor<nx::vms::api::PixelationSettings>(
            "pixelationSettings", nx::vms::api::PixelationSettings{}, this,
            [] { return tr("Pixelation settings"); });

    d->sessionLimitSAdaptor = new QnJsonResourcePropertyAdaptor<std::chrono::seconds>(
        Names::sessionLimitS, kDefaultSessionLimit,
        [](const std::chrono::seconds& value) { return value >= 0s; },
        this,
        [] { return tr("Authorization Session token lifetime (seconds)"); });

    d->useSessionLimitForCloudAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::useSessionLimitForCloud, false, this,
        [] { return tr("Apply session limit for Cloud tokens"); });

    d->sessionsLimitAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::sessionsLimit, 10'0000, [](const int& value) { return value >= 0; }, this,
        [] { return tr("Session token count limit on a single Server"); });

    d->sessionsLimitPerUserAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        Names::sessionsLimitPerUser, 5'000, [](const int& value) { return value >= 0; }, this,
        [] { return tr("Max session token count per user on single Server"); });

    d->remoteSessionUpdateSAdaptor = new QnJsonResourcePropertyAdaptor<std::chrono::seconds>(
        Names::remoteSessionUpdateS, 10s,
        [](const std::chrono::seconds& value) { return value > 0s; }, this,
        [] { return tr("Update interval for remote session token cache (other Servers and Cloud)"); });

    d->remoteSessionTimeoutSAdaptor = new QnJsonResourcePropertyAdaptor<std::chrono::seconds>(
        Names::remoteSessionTimeoutS, 6h,
        [](const std::chrono::seconds& value) { return value > 0s; }, this,
        [] { return tr("Timeout for remote session token cache (other Servers and Cloud)"); });

    d->defaultVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "defaultVideoCodec", "h263p", this, [] { return tr("Default video codec"); });

    d->defaultExportVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "defaultExportVideoCodec", "mpeg4", this,
        [] { return tr("Default codec for export video"); });

    d->lowQualityScreenVideoCodecAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "lowQualityScreenVideoCodec", "mpeg2video", this,
        [] { return tr("Low quality screen video codec"); });

    d->licenseServerAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::licenseServer, "https://licensing.vmsproxy.com", this,
        [] { return tr("License server"); });

    d->resourceFileUriAdaptor = new QnLexicalResourcePropertyAdaptor<nx::utils::Url>(
        Names::resourceFileUri, "https://resources.vmsproxy.com/resource_data.json", this,
        [] { return tr("URI for resource_data.json automatic update"); });

    d->maxEventLogRecordsAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "maxEventLogRecords", 100 * 1000, this,
        [] { return tr(
            "Maximum event log records to keep in the database. Real amount of undeleted records "
            "may be up to 20% higher than the specified value."); });

    d->forceLiveCacheForPrimaryStreamAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "forceLiveCacheForPrimaryStream", "auto", this,
        [] { return tr(
            "Whether or not to cache some frames for the primary stream. Values: "
            "'yes' - always enabled (may use a lot of RAM), "
            "'no' - always disabled except when required by the playback (e.g. HLS), "
            "'auto' - similar to 'no', but turned on when improving the user experience "
                "(e.g. when some Analytics plugin is working on the Camera)."); });

    d->metadataStorageChangePolicyAdaptor =
        new QnReflectLexicalResourcePropertyAdaptor<nx::vms::api::MetadataStorageChangePolicy>(
            "metadataStorageChangePolicy", nx::vms::api::MetadataStorageChangePolicy::keep, this,
            [] { return tr("Meta data storage change policy"); });

    d->targetPersistentUpdateStorageAdaptor =
        new QnJsonResourcePropertyAdaptor<api::PersistentUpdateStorage>(
            "targetPersistentUpdateStorage", api::PersistentUpdateStorage(), this,
            [] { return tr("Persistent Servers for update storage"); });

    d->installedPersistentUpdateStorageAdaptor =
        new QnJsonResourcePropertyAdaptor<api::PersistentUpdateStorage>(
            "installedPersistentUpdateStorage", api::PersistentUpdateStorage(), this,
            [] { return tr("Persistent Servers where updates are stored"); });

    d->specificFeaturesAdaptor = new QnJsonResourcePropertyAdaptor<std::map<QString, int>>(
        Names::specificFeatures, std::map<QString, int>{}, this,
        [] { return tr("VMS Server version specific features"); });

    d->cloudNotificationsLanguageAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::cloudNotificationsLanguage, "", this,
        [] { return tr("Language for Cloud notifications"); });

    d->additionalLocalFsTypesAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        "additionalLocalFsTypes", "", this,
        [] { return tr("Additional local FS storage types for recording"); });

    d->keepIoPortStateIntactOnInitializationAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "keepIoPortStateIntactOnInitialization", false, this,
        [] { return tr("Keep IO port state on when Server connects to the device"); });

    d->mediaBufferSizeKbAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "mediaBufferSizeKb", 256,
        [](auto v) { return v >= 10 && v <= 4 * 1024; }, this,
        [] { return tr("Media buffer size (KB, 10KB-4MB)"); });

    d->mediaBufferSizeForAudioOnlyDeviceKbAdaptor = new QnLexicalResourcePropertyAdaptor<int>(
        "mediaBufferSizeForAudioOnlyDeviceKb", 16,
        [](auto v) { return v >= 1 && v <= 1024; }, this,
        [] { return tr("Media buffer size for audio only devices (KB, 1KB-1MB)"); });

    d->forceAnalyticsDbStoragePermissionsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "forceAnalyticsDbStoragePermissions", true,  this,
        [] { return tr("Force analytics DB storage mount point permissions in case of failure"); });

    d->checkVideoStreamPeriodMsAdaptor = new QnJsonResourcePropertyAdaptor<std::chrono::milliseconds>(
        "checkVideoStreamPeriodMs", 10s, this,
        [] { return tr("Check video stream period (milliseconds)"); });

    d->storageEncryptionAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "storageEncryption", false, this, [] { return tr("Storage encryption enabled"); });

    d->currentStorageEncryptionKeyAdaptor = new QnLexicalResourcePropertyAdaptor<QByteArray>(
        "currentStorageEncryptionKey", QByteArray(), this,
        [] { return tr("Current storage encryption key"); });

    d->showServersInTreeForNonAdminsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "showServersInTreeForNonAdmins", true, this,
        [] { return tr("Show Servers in the Resource Tree for non-admins"); });

    d->serverHeaderAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
        Names::serverHeader, "$vmsName/$vmsVersion ($company) $compatibility", this,
        [] { return tr("HTTP header: Server, supported variables: $vmsName, $vmsVersion, $company, $compatibility"); });
    connect(
        d->serverHeaderAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        [this] { d->serverHeaderCache.reset(); });

    d->supportedOriginsAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
         Names::supportedOrigins, "*", this, [] { return tr("HTTP header: Origin"); });

    d->frameOptionsHeaderAdaptor = new QnLexicalResourcePropertyAdaptor<QString>(
         Names::frameOptionsHeader, "SAMEORIGIN", this, [] { return tr("HTTP header: X-Frame-Options"); });

    d->useHttpsOnlyForCamerasAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "useHttpsOnlyForCameras", false, this, [] { return tr("Use only HTTPS for cameras"); });

    d->securityForPowerUsersAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::securityForPowerUsers, true, this,
        [] { return tr("Allow Power User editing Security Settings"); });

    connect(d->securityForPowerUsersAdaptor, &QnAbstractResourcePropertyAdaptor::valueChanged,
        this, &SystemSettings::securityForPowerUsersChanged, Qt::QueuedConnection);

    d->insecureDeprecatedApiEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::insecureDeprecatedApiEnabled, false, this,
        [] { return tr("Enable deprecated API functions (insecure)"); });

    d->insecureDeprecatedApiInUseEnabledAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::insecureDeprecatedApiInUseEnabled, false, this,
        []
        {
            return nx::format(tr(
                "Enable deprecated API functions currently used by %1 software (insecure)",
                "%1 is a company name"),
                nx::branding::company());
        });

    d->exposeServerEndpointsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        Names::exposeServerEndpoints, true, this,
        []() { return tr("Expose IP addresses for autodiscovery"); });

    d->showMouseTimelinePreviewAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "showMouseTimelinePreview", true, this, [] { return tr("Show mouse timeline preview"); });

    d->ldapAdaptor = new QnJsonResourcePropertyAdaptor<nx::vms::api::LdapSettings>(
        Names::ldap, nx::vms::api::LdapSettings(), this,
        [] { return tr("LDAP settings"); });

    d->cloudPollingIntervalSAdaptor = new QnJsonResourcePropertyAdaptor<std::chrono::seconds>(
        Names::cloudPollingIntervalS, 60s,
        [](const std::chrono::seconds& value) { return value >= 1s && value <= 10min; },
        this,
        []
        {
            return tr("Interval between the Cloud polling HTTP requests to synchronize the data.");
        });

    d->allowRegisteringIntegrationsAdaptor = new QnLexicalResourcePropertyAdaptor<bool>(
        "allowRegisteringIntegrations", /*defaultValue*/ true, this,
            []()
            {
                return tr("Enable or disable the creation of new Integration registration requests");
            });

    connect(
        d->systemNameAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::systemNameChanged,
        Qt::QueuedConnection);

    connect(
        d->localSystemIdAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::localSystemIdChanged,
        Qt::DirectConnection);

    connect(
        d->disabledVendorsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::disabledVendorsChanged,
        Qt::QueuedConnection);

    connect(
        d->auditTrailEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::auditTrailEnableChanged,
        Qt::QueuedConnection);

    connect(
        d->auditTrailPeriodDaysAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::auditTrailPeriodDaysChanged,
        Qt::QueuedConnection);

    connect(
        d->trafficEncryptionForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::trafficEncryptionForcedChanged,
        Qt::QueuedConnection);

    connect(
        d->videoTrafficEncryptionForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::videoTrafficEncryptionForcedChanged,
        Qt::QueuedConnection);

    connect(
        d->eventLogPeriodDaysAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::eventLogPeriodDaysChanged,
        Qt::QueuedConnection);

    connect(
        d->cameraSettingsOptimizationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cameraSettingsOptimizationChanged,
        Qt::QueuedConnection);

    connect(
        d->useHttpsOnlyForCamerasAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useHttpsOnlyForCamerasChanged,
        Qt::QueuedConnection);

    connect(
        d->autoUpdateThumbnailsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::autoUpdateThumbnailsChanged,
        Qt::QueuedConnection);

    connect(
        d->maxSceneItemsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::maxSceneItemsChanged,
        Qt::DirectConnection); //< I need this one now :)

    connect(
        d->useTextEmailFormatAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useTextEmailFormatChanged,
        Qt::QueuedConnection);

    connect(
        d->useWindowsEmailLineFeedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useWindowsEmailLineFeedChanged,
        Qt::QueuedConnection);

    connect(
        d->autoDiscoveryEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::autoDiscoveryChanged,
        Qt::QueuedConnection);

    connect(
        d->autoDiscoveryResponseEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::autoDiscoveryChanged,
        Qt::QueuedConnection);

    connect(
        d->updateNotificationsEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::updateNotificationsChanged,
        Qt::QueuedConnection);

    connect(
        d->upnpPortMappingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::upnpPortMappingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        d->cloudConnectUdpHolePunchingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudConnectUdpHolePunchingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        d->cloudConnectRelayingEnabledAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudConnectRelayingEnabledChanged,
        Qt::QueuedConnection);

    connect(
        d->cloudConnectRelayingOverSslForcedAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudConnectRelayingOverSslForcedChanged,
        Qt::QueuedConnection);

    connect(
        d->watermarkSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::watermarkChanged,
        Qt::QueuedConnection);

    connect(
        d->pixelationSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::pixelationSettingsChanged,
        Qt::QueuedConnection);

    connect(
        d->sessionLimitSAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::sessionTimeoutChanged,
        Qt::QueuedConnection);

    connect(
        d->useSessionLimitForCloudAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::sessionTimeoutChanged,
        Qt::QueuedConnection);

    connect(
        d->sessionsLimitAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::sessionsLimitChanged,
        Qt::DirectConnection);

    connect(
        d->sessionsLimitPerUserAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::sessionsLimitPerUserChanged,
        Qt::DirectConnection);

    connect(
        d->remoteSessionUpdateSAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::remoteSessionUpdateChanged,
        Qt::QueuedConnection);

    connect(
        d->remoteSessionTimeoutSAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::remoteSessionUpdateChanged,
        Qt::QueuedConnection);

    connect(
        d->storageEncryptionAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::useStorageEncryptionChanged,
        Qt::QueuedConnection);

    connect(
        d->currentStorageEncryptionKeyAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::currentStorageEncryptionKeyChanged,
        Qt::QueuedConnection);

    connect(
        d->showServersInTreeForNonAdminsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::showServersInTreeForNonAdminsChanged,
        Qt::QueuedConnection);

    connect(
        d->customReleaseListUrlAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::customReleaseListUrlChanged,
        Qt::QueuedConnection);

    connect(
        d->targetUpdateInformationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::targetUpdateInformationChanged,
        Qt::QueuedConnection);

    connect(
        d->installedUpdateInformationAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::installedUpdateInformationChanged,
        Qt::QueuedConnection);

    connect(
        d->downloaderPeersAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::downloaderPeersChanged,
        Qt::QueuedConnection);

    connect(
        d->targetPersistentUpdateStorageAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::targetPersistentUpdateStorageChanged,
        Qt::QueuedConnection);

    connect(
        d->installedPersistentUpdateStorageAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::installedPersistentUpdateStorageChanged,
        Qt::QueuedConnection);

    connect(
        d->clientUpdateSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::clientUpdateSettingsChanged,
        Qt::QueuedConnection);

    connect(
        d->cloudNotificationsLanguageAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudNotificationsLanguageChanged,
        Qt::QueuedConnection);

    connect(
        d->backupSettingsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::backupSettingsChanged,
        Qt::QueuedConnection);

    connect(
        d->showMouseTimelinePreviewAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::showMouseTimelinePreviewChanged,
        Qt::QueuedConnection);

    connect(
        d->ldapAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::ldapSettingsChanged,
        Qt::DirectConnection);

    connect(
        d->masterCloudSyncListAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::masterCloudSyncChanged,
        Qt::QueuedConnection);

    connect(
        d->cloudPollingIntervalSAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::cloudPollingIntervalChanged,
        Qt::QueuedConnection);

    connect(
        d->allowRegisteringIntegrationsAdaptor,
        &QnAbstractResourcePropertyAdaptor::valueChanged,
        this,
        &SystemSettings::allowRegisteringIntegrationsChanged,
        Qt::QueuedConnection);

    AdaptorList result;
    result
        << d->systemNameAdaptor
        << d->localSystemIdAdaptor
        << d->lastMergeMasterIdAdaptor
        << d->lastMergeSlaveIdAdaptor
        << d->disabledVendorsAdaptor
        << d->cameraSettingsOptimizationAdaptor
        << d->autoUpdateThumbnailsAdaptor
        << d->maxSceneItemsAdaptor
        << d->useTextEmailFormatAdaptor
        << d->useWindowsEmailLineFeedAdaptor
        << d->auditTrailEnabledAdaptor
        << d->auditTrailPeriodDaysAdaptor
        << d->trafficEncryptionForcedAdaptor
        << d->videoTrafficEncryptionForcedAdaptor
        << d->exposeDeviceCredentialsAdaptor
        << d->useHttpsOnlyForCamerasAdaptor
        << d->securityForPowerUsersAdaptor
        << d->insecureDeprecatedApiEnabledAdaptor
        << d->insecureDeprecatedApiInUseEnabledAdaptor
        << d->eventLogPeriodDaysAdaptor
        << d->autoDiscoveryEnabledAdaptor
        << d->autoDiscoveryResponseEnabledAdaptor
        << d->updateNotificationsEnabledAdaptor
        << d->backupSettingsAdaptor
        << d->upnpPortMappingEnabledAdaptor
        << d->cloudHostAdaptor
        << d->crossdomainEnabledAdaptor
        << d->arecontRtspEnabledAdaptor
        << d->sequentialFlirOnvifSearcherEnabledAdaptor
        << d->maxP2pQueueSizeBytesAdaptor
        << d->maxP2pAllClientsSizeBytesAdaptor
        << d->maxRecordQueueSizeBytesAdaptor
        << d->maxRecordQueueSizeElementsAdaptor
        << d->rtpTimeoutMsAdaptor
        << d->maxRtspConnectDurationSecondsAdaptor
        << d->cloudConnectUdpHolePunchingEnabledAdaptor
        << d->cloudConnectRelayingEnabledAdaptor
        << d->cloudConnectRelayingOverSslForcedAdaptor
        << d->enableEdgeRecordingAdaptor
        << d->webSocketEnabledAdaptor
        << d->maxRemoteArchiveSynchronizationThreadsAdaptor
        << d->customReleaseListUrlAdaptor
        << d->targetUpdateInformationAdaptor
        << d->installedUpdateInformationAdaptor
        << d->maxVirtualCameraArchiveSynchronizationThreadsAdaptor
        << d->watermarkSettingsAdaptor
        << d->pixelationSettingsAdaptor
        << d->sessionLimitSAdaptor
        << d->useSessionLimitForCloudAdaptor
        << d->sessionsLimitAdaptor
        << d->sessionsLimitPerUserAdaptor
        << d->remoteSessionUpdateSAdaptor
        << d->remoteSessionTimeoutSAdaptor
        << d->defaultVideoCodecAdaptor
        << d->defaultExportVideoCodecAdaptor
        << d->downloaderPeersAdaptor
        << d->clientUpdateSettingsAdaptor
        << d->lowQualityScreenVideoCodecAdaptor
        << d->licenseServerAdaptor
        << d->resourceFileUriAdaptor
        << d->maxHttpTranscodingSessionsAdaptor
        << d->maxEventLogRecordsAdaptor
        << d->forceLiveCacheForPrimaryStreamAdaptor
        << d->metadataStorageChangePolicyAdaptor
        << d->maxRtpRetryCountAdaptor
        << d->targetPersistentUpdateStorageAdaptor
        << d->installedPersistentUpdateStorageAdaptor
        << d->specificFeaturesAdaptor
        << d->cloudNotificationsLanguageAdaptor
        << d->additionalLocalFsTypesAdaptor
        << d->keepIoPortStateIntactOnInitializationAdaptor
        << d->mediaBufferSizeKbAdaptor
        << d->mediaBufferSizeForAudioOnlyDeviceKbAdaptor
        << d->forceAnalyticsDbStoragePermissionsAdaptor
        << d->checkVideoStreamPeriodMsAdaptor
        << d->storageEncryptionAdaptor
        << d->currentStorageEncryptionKeyAdaptor
        << d->showServersInTreeForNonAdminsAdaptor
        << d->serverHeaderAdaptor
        << d->supportedOriginsAdaptor
        << d->frameOptionsHeaderAdaptor
        << d->showMouseTimelinePreviewAdaptor
        << d->ldapAdaptor
        << d->exposeServerEndpointsAdaptor
        << d->cloudPollingIntervalSAdaptor
        << d->allowRegisteringIntegrationsAdaptor
    ;

    return result;
}

QString SystemSettings::disabledVendors() const
{
    return d->disabledVendorsAdaptor->value();
}

QSet<QString> SystemSettings::disabledVendorsSet() const
{
    return parseDisabledVendors(disabledVendors());
}

void SystemSettings::setDisabledVendors(QString disabledVendors)
{
    d->disabledVendorsAdaptor->setValue(disabledVendors);
}

bool SystemSettings::isCameraSettingsOptimizationEnabled() const
{
    return d->cameraSettingsOptimizationAdaptor->value();
}

void SystemSettings::setCameraSettingsOptimizationEnabled(bool cameraSettingsOptimizationEnabled)
{
    d->cameraSettingsOptimizationAdaptor->setValue(cameraSettingsOptimizationEnabled);
}

bool SystemSettings::isAutoUpdateThumbnailsEnabled() const
{
    return d->autoUpdateThumbnailsAdaptor->value();
}

void SystemSettings::setAutoUpdateThumbnailsEnabled(bool value)
{
    d->autoUpdateThumbnailsAdaptor->setValue(value);
}

int SystemSettings::maxSceneItemsOverride() const
{
    return d->maxSceneItemsAdaptor->value();
}

void SystemSettings::setMaxSceneItemsOverride(int value)
{
    d->maxSceneItemsAdaptor->setValue(value);
}

int SystemSettings::maxSceneItemsOverrideRawValue() const
{
    return d->maxSceneItemsAdaptor->rawValue().toInt();
}

bool SystemSettings::isUseTextEmailFormat() const
{
    return d->useTextEmailFormatAdaptor->value();
}

void SystemSettings::setUseTextEmailFormat(bool value)
{
    d->useTextEmailFormatAdaptor->setValue(value);
}

bool SystemSettings::isUseWindowsEmailLineFeed() const
{
    return d->useWindowsEmailLineFeedAdaptor->value();
}

void SystemSettings::setUseWindowsEmailLineFeed(bool value)
{
    d->useWindowsEmailLineFeedAdaptor->setValue(value);
}

bool SystemSettings::isAuditTrailEnabled() const
{
    return d->auditTrailEnabledAdaptor->value();
}

void SystemSettings::setAuditTrailEnabled(bool value)
{
    d->auditTrailEnabledAdaptor->setValue(value);
}

std::chrono::days SystemSettings::auditTrailPeriodDays() const
{
    return std::chrono::days(d->auditTrailPeriodDaysAdaptor->value());
}

std::chrono::days SystemSettings::eventLogPeriodDays() const
{
    return std::chrono::days(d->eventLogPeriodDaysAdaptor->value());
}

bool SystemSettings::isTrafficEncryptionForced() const
{
    return d->trafficEncryptionForcedAdaptor->value();
}

void SystemSettings::setTrafficEncryptionForced(bool value)
{
    d->trafficEncryptionForcedAdaptor->setValue(value);
}

bool SystemSettings::isVideoTrafficEncryptionForced() const
{
    return d->videoTrafficEncryptionForcedAdaptor->value();
}

void SystemSettings::setVideoTrafficEncryptionForced(bool value)
{
    d->videoTrafficEncryptionForcedAdaptor->setValue(value);
}

bool SystemSettings::exposeDeviceCredentials() const
{
    return d->exposeDeviceCredentialsAdaptor->value();
}

void SystemSettings::setExposeDeviceCredentials(bool value)
{
    d->exposeDeviceCredentialsAdaptor->setValue(value);
}

bool SystemSettings::isAutoDiscoveryEnabled() const
{
    return d->autoDiscoveryEnabledAdaptor->value();
}

void SystemSettings::setAutoDiscoveryEnabled(bool enabled)
{
    d->autoDiscoveryEnabledAdaptor->setValue(enabled);
}

bool SystemSettings::isAutoDiscoveryResponseEnabled() const
{
    return d->autoDiscoveryResponseEnabledAdaptor->value();
}

void SystemSettings::setAutoDiscoveryResponseEnabled(bool enabled)
{
    d->autoDiscoveryResponseEnabledAdaptor->setValue(enabled);
}

bool SystemSettings::isAllowRegisteringIntegrationsEnabled() const
{
    return d->allowRegisteringIntegrationsAdaptor->value();
}

void SystemSettings::setAllowRegisteringIntegrationsEnabled(bool value)
{
    d->allowRegisteringIntegrationsAdaptor->setValue(value);
}

void SystemSettings::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    if (!d->admin || resource != d->admin)
        return;

    NX_MUTEX_LOCKER locker( &d->mutex );
    d->admin.reset();

    for (auto adaptor: d->allAdaptors)
        adaptor->setResource(QnResourcePtr());
}

nx::vms::api::LdapSettings SystemSettings::ldap() const
{
    return d->ldapAdaptor->value();
}

void SystemSettings::setLdap(nx::vms::api::LdapSettings settings)
{
    d->ldapAdaptor->setValue(std::move(settings));
}

QnEmailSettings SystemSettings::emailSettings() const
{
    return d->emailSettingsAdaptor->value();
}

void SystemSettings::setEmailSettings(const QnEmailSettings& emailSettings)
{
    d->emailSettingsAdaptor->setValue(static_cast<nx::vms::api::EmailSettings>(emailSettings));
}

void SystemSettings::synchronizeNow()
{
    for (auto adaptor: d->allAdaptors)
        adaptor->saveToResource();

    NX_MUTEX_LOCKER locker(&d->mutex);
    if (!d->admin)
        return;

    resourcePropertyDictionary()->saveParamsAsync(d->admin->getId());
}

bool SystemSettings::resynchronizeNowSync(nx::utils::MoveOnlyFunc<
    bool(const QString& paramName, const QString& paramValue)> filter)
{
    {
        NX_MUTEX_LOCKER locker(&d->mutex);
        NX_ASSERT(d->admin, "Invalid sync state");
        if (!d->admin)
            return false;
        resourcePropertyDictionary()->markAllParamsDirty(d->admin->getId(), std::move(filter));
    }
    return synchronizeNowSync();
}

bool SystemSettings::synchronizeNowSync()
{
    for (auto adaptor : d->allAdaptors)
        adaptor->saveToResource();

    NX_MUTEX_LOCKER locker(&d->mutex);
    NX_ASSERT(d->admin, "Invalid sync state");
    if (!d->admin)
        return false;
    return resourcePropertyDictionary()->saveParams(d->admin->getId());
}

bool SystemSettings::takeFromSettings(QSettings* settings, const QnResourcePtr& mediaServer)
{
    bool changed = false;
    for (const auto adapter: d->allAdaptors)
        changed |= adapter->takeFromSettings(settings, "system.");

    // TODO: These are probably deprecated and should be removed at some point in the future.
    changed |= d->statisticsAllowedAdaptor->takeFromSettings(settings, "");
    changed |= d->statisticsReportTimeCycleAdaptor->takeFromSettings(settings, "");
    changed |= d->statisticsReportUpdateDelayAdaptor->takeFromSettings(settings, "");
    changed |= d->statisticsReportServerApiAdaptor->takeFromSettings(settings, "");
    changed |= d->clientStatisticsSettingsUrlAdaptor->takeFromSettings(settings, "");
    changed |= d->cloudSystemIDAdaptor->takeFromSettings(settings, "");
    changed |= d->cloudAuthKeyAdaptor->takeFromSettings(settings, "");

    if (!changed)
        return false;

    if (!synchronizeNowSync())
        return false;

    settings->sync();
    return true;
}

bool SystemSettings::isUpdateNotificationsEnabled() const
{
    return d->updateNotificationsEnabledAdaptor->value();
}

void SystemSettings::setUpdateNotificationsEnabled(bool updateNotificationsEnabled)
{
    d->updateNotificationsEnabledAdaptor->setValue(updateNotificationsEnabled);
}

nx::vms::api::BackupSettings SystemSettings::backupSettings() const
{
    return d->backupSettingsAdaptor->value();
}

void SystemSettings::setBackupSettings(const nx::vms::api::BackupSettings& value)
{
    d->backupSettingsAdaptor->setValue(value);
}

bool SystemSettings::isStatisticsAllowed() const
{
    return d->statisticsAllowedAdaptor->value();
}

void SystemSettings::setStatisticsAllowed(bool value)
{
    d->statisticsAllowedAdaptor->setValue(value);
}

QDateTime SystemSettings::statisticsReportLastTime() const
{
    return QDateTime::fromString(d->statisticsReportLastTimeAdaptor->value(), Qt::ISODate);
}

void SystemSettings::setStatisticsReportLastTime(const QDateTime& value)
{
    d->statisticsReportLastTimeAdaptor->setValue(value.toString(Qt::ISODate));
}

QString SystemSettings::statisticsReportLastVersion() const
{
    return d->statisticsReportLastVersionAdaptor->value();
}

void SystemSettings::setStatisticsReportLastVersion(const QString& value)
{
    d->statisticsReportLastVersionAdaptor->setValue(value);
}

int SystemSettings::statisticsReportLastNumber() const
{
    return d->statisticsReportLastNumberAdaptor->value();
}

void SystemSettings::setStatisticsReportLastNumber(int value)
{
    d->statisticsReportLastNumberAdaptor->setValue(value);
}

QString SystemSettings::statisticsReportTimeCycle() const
{
    return d->statisticsReportTimeCycleAdaptor->value();
}

void SystemSettings::setStatisticsReportTimeCycle(const QString& value)
{
    d->statisticsReportTimeCycleAdaptor->setValue(value);
}

QString SystemSettings::statisticsReportUpdateDelay() const
{
    return d->statisticsReportUpdateDelayAdaptor->value();
}

void SystemSettings::setStatisticsReportUpdateDelay(const QString& value)
{
    d->statisticsReportUpdateDelayAdaptor->setValue(value);
}

bool SystemSettings::isUpnpPortMappingEnabled() const
{
    return d->upnpPortMappingEnabledAdaptor->value();
}

void SystemSettings::setUpnpPortMappingEnabled(bool value)
{
    d->upnpPortMappingEnabledAdaptor->setValue(value);
}

nx::Uuid SystemSettings::localSystemId() const
{
    NX_VERBOSE(this, "Providing local System ID %1", d->localSystemIdAdaptor->value());
    return d->localSystemIdAdaptor->value();
}

void SystemSettings::setLocalSystemId(const nx::Uuid& value)
{
    if (const auto oldValue = d->localSystemIdAdaptor->value(); oldValue != value)
    {
        NX_DEBUG(this, "Changing local System ID from %1 to %2", oldValue, value);
        d->localSystemIdAdaptor->setValue(value);
    }
}

nx::Uuid SystemSettings::lastMergeMasterId() const
{
    return nx::Uuid(d->lastMergeMasterIdAdaptor->value());
}

void SystemSettings::setLastMergeMasterId(const nx::Uuid& value)
{
    d->lastMergeMasterIdAdaptor->setValue(value.toString());
}

nx::Uuid SystemSettings::lastMergeSlaveId() const
{
    return nx::Uuid(d->lastMergeSlaveIdAdaptor->value());
}

void SystemSettings::setLastMergeSlaveId(const nx::Uuid& value)
{
    d->lastMergeSlaveIdAdaptor->setValue(value.toString());
}

nx::utils::Url SystemSettings::clientStatisticsSettingsUrl() const
{
    return nx::utils::Url::fromUserInput(d->clientStatisticsSettingsUrlAdaptor->value().trimmed());
}

std::chrono::seconds SystemSettings::deviceStorageInfoUpdateInterval() const
{
    return d->deviceStorageInfoUpdateIntervalSAdaptor->value();
}

void SystemSettings::setDeviceStorageInfoUpdateInterval(std::chrono::seconds value)
{
    d->deviceStorageInfoUpdateIntervalSAdaptor->setValue(value);
}

QString SystemSettings::statisticsReportServerApi() const
{
    return d->statisticsReportServerApiAdaptor->value();
}

void SystemSettings::setStatisticsReportServerApi(const QString& value)
{
    d->statisticsReportServerApiAdaptor->setValue(value);
}

QString SystemSettings::crashReportServerApi() const
{
    return d->crashReportServerApiAdaptor->value();
}

void SystemSettings::setCrashReportServerApi(const QString& value)
{
    d->crashReportServerApiAdaptor->setValue(value);
}

int SystemSettings::maxEventLogRecords() const
{
    return d->maxEventLogRecordsAdaptor->value();
}

void SystemSettings::setMaxEventLogRecords(int value)
{
    d->maxEventLogRecordsAdaptor->setValue(value);
}

std::chrono::seconds SystemSettings::aliveUpdateInterval() const
{
    return std::chrono::seconds(d->ec2AliveUpdateIntervalSecAdaptor->value());
}

void SystemSettings::setAliveUpdateInterval(std::chrono::seconds newInterval) const
{
    d->ec2AliveUpdateIntervalSecAdaptor->setValue(newInterval.count());
}

bool SystemSettings::isTimeSynchronizationEnabled() const
{
    return d->timeSynchronizationEnabledAdaptor->value();
}

void SystemSettings::setTimeSynchronizationEnabled(bool value)
{
    d->timeSynchronizationEnabledAdaptor->setValue(value);
}

nx::Uuid SystemSettings::primaryTimeServer() const
{
    return d->primaryTimeServerAdaptor->value();
}

void SystemSettings::setPrimaryTimeServer(const nx::Uuid& value)
{
    d->primaryTimeServerAdaptor->setValue(value);
}

std::chrono::milliseconds SystemSettings::maxDifferenceBetweenSynchronizedAndInternetTime() const
{
    return std::chrono::milliseconds(
        d->maxDifferenceBetweenSynchronizedAndInternetTimeAdaptor->value());
}

std::chrono::milliseconds SystemSettings::maxDifferenceBetweenSynchronizedAndLocalTime() const
{
    return std::chrono::milliseconds(
        d->maxDifferenceBetweenSynchronizedAndLocalTimeMsAdaptor->value());
}

std::chrono::milliseconds SystemSettings::osTimeChangeCheckPeriod() const
{
    return d->osTimeChangeCheckPeriodMsAdaptor->value();
}

void SystemSettings::setOsTimeChangeCheckPeriod(std::chrono::milliseconds value)
{
    d->osTimeChangeCheckPeriodMsAdaptor->setValue(value);
}

std::chrono::milliseconds SystemSettings::syncTimeExchangePeriod() const
{
    return std::chrono::milliseconds(d->syncTimeExchangePeriodAdaptor->value());
}

void SystemSettings::setSyncTimeExchangePeriod(std::chrono::milliseconds value)
{
    d->syncTimeExchangePeriodAdaptor->setValue(value.count());
}

std::chrono::milliseconds SystemSettings::syncTimeEpsilon() const
{
    return std::chrono::milliseconds(d->syncTimeEpsilonAdaptor->value());
}

void SystemSettings::setSyncTimeEpsilon(std::chrono::milliseconds value)
{
    d->syncTimeEpsilonAdaptor->setValue(value.count());
}

QString SystemSettings::cloudAccountName() const
{
    return d->cloudAccountNameAdaptor->value();
}

void SystemSettings::setCloudAccountName(const QString& value)
{
    d->cloudAccountNameAdaptor->setValue(value);
}

nx::Uuid SystemSettings::organizationId() const
{
    return d->organizationIdAdaptor->value();
}

void SystemSettings::setOrganizationId(const nx::Uuid& value)
{
    d->organizationIdAdaptor->setValue(value);
}

QString SystemSettings::cloudSystemId() const
{
    return d->cloudSystemIDAdaptor->value();
}

void SystemSettings::setCloudSystemId(const QString& value)
{
    d->cloudSystemIDAdaptor->setValue(value);
}

QString SystemSettings::cloudAuthKey() const
{
    return d->cloudAuthKeyAdaptor->value();
}

void SystemSettings::setCloudAuthKey(const QString& value)
{
    d->cloudAuthKeyAdaptor->setValue(value);
}

QString SystemSettings::systemName() const
{
    return d->systemNameAdaptor->value();
}

void SystemSettings::setSystemName(const QString& value)
{
    d->systemNameAdaptor->setValue(value);
}

void SystemSettings::resetCloudParams()
{
    setCloudAccountName(QString());
    setCloudSystemId(QString());
    setCloudAuthKey(QString());
    setOrganizationId(nx::Uuid());
}

void SystemSettings::resetCloudParamsWithLowPriority()
{
    const std::array<QString, 4> names = {
        Names::cloudAccountName,
        Names::cloudSystemID,
        Names::cloudAuthKey,
        Names::organizationId};

    nx::vms::api::ResourceParamWithRefDataList params;
    for (const auto& name: names)
    {
        nx::vms::api::ResourceParamWithRefData param;
        param.resourceId = QnUserResource::kAdminGuid;
        param.name = name;
        params.emplace_back(std::move(param));
    }

    auto ec2Connection = systemContext()->messageBusConnection();
    auto manager = ec2Connection->getMiscManager(nx::network::rest::kSystemSession);
    manager->updateKvPairsWithLowPrioritySync(std::move(params));
}

QString SystemSettings::cloudHost() const
{
    return d->cloudHostAdaptor->value();
}

void SystemSettings::setCloudHost(const QString& value)
{
    d->cloudHostAdaptor->setValue(value);
}

bool SystemSettings::crossdomainEnabled() const
{
    return d->crossdomainEnabledAdaptor->value();
}

void SystemSettings::setCrossdomainEnabled(bool value)
{
    d->crossdomainEnabledAdaptor->setValue(value);
}

bool SystemSettings::arecontRtspEnabled() const
{
    return d->arecontRtspEnabledAdaptor->value();
}

void SystemSettings::setArecontRtspEnabled(bool newVal) const
{
    d->arecontRtspEnabledAdaptor->setValue(newVal);
}

int SystemSettings::maxRtpRetryCount() const
{
    return d->maxRtpRetryCountAdaptor->value();
}

void SystemSettings::setMaxRtpRetryCount(int newVal)
{
    d->maxRtpRetryCountAdaptor->setValue(newVal);
}

bool SystemSettings::sequentialFlirOnvifSearcherEnabled() const
{
    return d->sequentialFlirOnvifSearcherEnabledAdaptor->value();
}

void SystemSettings::setSequentialFlirOnvifSearcherEnabled(bool newVal)
{
    d->sequentialFlirOnvifSearcherEnabledAdaptor->setValue(newVal);
}

int SystemSettings::rtpFrameTimeoutMs() const
{
    return d->rtpTimeoutMsAdaptor->value().count();
}

void SystemSettings::setRtpFrameTimeoutMs(int newValue)
{
    d->rtpTimeoutMsAdaptor->setValue(std::chrono::milliseconds(newValue));
}

std::chrono::seconds SystemSettings::maxRtspConnectDuration() const
{
    return std::chrono::seconds(d->maxRtspConnectDurationSecondsAdaptor->value());
}

void SystemSettings::setMaxRtspConnectDuration(std::chrono::seconds newValue)
{
    d->maxRtspConnectDurationSecondsAdaptor->setValue(newValue.count());
}

int SystemSettings::maxP2pQueueSizeBytes() const
{
    return d->maxP2pQueueSizeBytesAdaptor->value();
}

qint64 SystemSettings::maxP2pQueueSizeForAllClientsBytes() const
{
    return d->maxP2pAllClientsSizeBytesAdaptor->value();
}

int SystemSettings::maxRecorderQueueSizeBytes() const
{
    return d->maxRecordQueueSizeBytesAdaptor->value();
}

int SystemSettings::maxHttpTranscodingSessions() const
{
    return d->maxHttpTranscodingSessionsAdaptor->value();
}

int SystemSettings::maxRecorderQueueSizePackets() const
{
    return d->maxRecordQueueSizeElementsAdaptor->value();
}

bool SystemSettings::isEdgeRecordingEnabled() const
{
    return d->enableEdgeRecordingAdaptor->value();
}

void SystemSettings::setEdgeRecordingEnabled(bool enabled)
{
    d->enableEdgeRecordingAdaptor->setValue(enabled);
}

nx::utils::Url SystemSettings::customReleaseListUrl() const
{
    return d->customReleaseListUrlAdaptor->value();
}

void SystemSettings::setCustomReleaseListUrl(const nx::utils::Url& url)
{
    d->customReleaseListUrlAdaptor->setValue(url);
}

QByteArray SystemSettings::targetUpdateInformation() const
{
    return d->targetUpdateInformationAdaptor->value();
}

bool SystemSettings::isWebSocketEnabled() const
{
    return d->webSocketEnabledAdaptor->value();
}

void SystemSettings::setWebSocketEnabled(bool enabled)
{
    d->webSocketEnabledAdaptor->setValue(enabled);
}

void SystemSettings::setTargetUpdateInformation(const QByteArray& updateInformation)
{
    d->targetUpdateInformationAdaptor->setValue(updateInformation);
}

api::PersistentUpdateStorage SystemSettings::targetPersistentUpdateStorage() const
{
    return d->targetPersistentUpdateStorageAdaptor->value();
}

void SystemSettings::setTargetPersistentUpdateStorage(
    const api::PersistentUpdateStorage& data)
{
    d->targetPersistentUpdateStorageAdaptor->setValue(data);
}

api::PersistentUpdateStorage SystemSettings::installedPersistentUpdateStorage() const
{
    return d->installedPersistentUpdateStorageAdaptor->value();
}

void SystemSettings::setInstalledPersistentUpdateStorage(
    const api::PersistentUpdateStorage& data)
{
    d->installedPersistentUpdateStorageAdaptor->setValue(data);
}

QByteArray SystemSettings::installedUpdateInformation() const
{
    return d->installedUpdateInformationAdaptor->value();
}

void SystemSettings::setInstalledUpdateInformation(const QByteArray& updateInformation)
{
    d->installedUpdateInformationAdaptor->setValue(updateInformation);
}

FileToPeerList SystemSettings::downloaderPeers() const
{
    return d->downloaderPeersAdaptor->value();
}

void SystemSettings::setdDownloaderPeers(const FileToPeerList& downloaderPeers)
{
    d->downloaderPeersAdaptor->setValue(downloaderPeers);
}

api::ClientUpdateSettings SystemSettings::clientUpdateSettings() const
{
    return d->clientUpdateSettingsAdaptor->value();
}

void SystemSettings::setClientUpdateSettings(const api::ClientUpdateSettings& settings)
{
    d->clientUpdateSettingsAdaptor->setValue(settings);
}

int SystemSettings::maxRemoteArchiveSynchronizationThreads() const
{
    return d->maxRemoteArchiveSynchronizationThreadsAdaptor->value();
}

void SystemSettings::setMaxRemoteArchiveSynchronizationThreads(int newValue)
{
    d->maxRemoteArchiveSynchronizationThreadsAdaptor->setValue(newValue);
}

int SystemSettings::maxVirtualCameraArchiveSynchronizationThreads() const
{
    return d->maxVirtualCameraArchiveSynchronizationThreadsAdaptor->value();
}

void SystemSettings::setMaxVirtualCameraArchiveSynchronizationThreads(int newValue)
{
    d->maxVirtualCameraArchiveSynchronizationThreadsAdaptor->setValue(newValue);
}

std::chrono::seconds SystemSettings::proxyConnectTimeout() const
{
    return std::chrono::seconds(d->proxyConnectTimeoutSecAdaptor->value());
}

api::ProxyConnectionAccessPolicy SystemSettings::proxyConnectionAccessPolicy() const
{
    return d->proxyConnectionAccessPolicyAdaptor->value();
}

void SystemSettings::setProxyConnectionAccessPolicy(api::ProxyConnectionAccessPolicy value)
{
    d->proxyConnectionAccessPolicyAdaptor->setValue(value);
}

bool SystemSettings::cloudConnectUdpHolePunchingEnabled() const
{
    return d->cloudConnectUdpHolePunchingEnabledAdaptor->value();
}

bool SystemSettings::cloudConnectRelayingEnabled() const
{
    return d->cloudConnectRelayingEnabledAdaptor->value();
}

bool SystemSettings::cloudConnectRelayingOverSslForced() const
{
    return d->cloudConnectRelayingOverSslForcedAdaptor->value();
}

nx::vms::api::WatermarkSettings SystemSettings::watermarkSettings() const
{
    return d->watermarkSettingsAdaptor->value();
}

void SystemSettings::setWatermarkSettings(const nx::vms::api::WatermarkSettings& settings) const
{
    d->watermarkSettingsAdaptor->setValue(settings);
}

nx::vms::api::PixelationSettings SystemSettings::pixelationSettings() const
{
    return d->pixelationSettingsAdaptor->value();
}

void SystemSettings::setPixelationSettings(const nx::vms::api::PixelationSettings& settings)
{
    d->pixelationSettingsAdaptor->setValue(settings);
}

std::optional<std::chrono::seconds> SystemSettings::sessionTimeoutLimit() const
{
    if (auto seconds = d->sessionLimitSAdaptor->value(); seconds > 0s)
        return seconds;
    return {};
}

void SystemSettings::setSessionTimeoutLimit(std::optional<std::chrono::seconds> value)
{
    d->sessionLimitSAdaptor->setValue(value.value_or(0s));
}

bool SystemSettings::useSessionTimeoutLimitForCloud() const
{
    return d->useSessionLimitForCloudAdaptor->value();
}

void SystemSettings::setUseSessionTimeoutLimitForCloud(bool value)
{
    d->useSessionLimitForCloudAdaptor->setValue(value);
}

int SystemSettings::sessionsLimit() const
{
    return d->sessionsLimitAdaptor->value();
}

void SystemSettings::setSessionsLimit(int value)
{
    d->sessionsLimitAdaptor->setValue(value);
}

int SystemSettings::sessionsLimitPerUser() const
{
    return d->sessionsLimitPerUserAdaptor->value();
}

void SystemSettings::setSessionsLimitPerUser(int value)
{
    d->sessionsLimitPerUserAdaptor->setValue(value);
}

std::chrono::seconds SystemSettings::remoteSessionUpdate() const
{
    return d->remoteSessionUpdateSAdaptor->value();
}

void SystemSettings::setRemoteSessionUpdate(std::chrono::seconds value)
{
    d->remoteSessionUpdateSAdaptor->setValue(value);
}

std::chrono::seconds SystemSettings::remoteSessionTimeout() const
{
    return d->remoteSessionTimeoutSAdaptor->value();
}

void SystemSettings::setRemoteSessionTimeout(std::chrono::seconds value)
{
    d->remoteSessionTimeoutSAdaptor->setValue(value);
}

QString SystemSettings::defaultVideoCodec() const
{
    return d->defaultVideoCodecAdaptor->value();
}

void SystemSettings::setDefaultVideoCodec(const QString& value)
{
    d->defaultVideoCodecAdaptor->setValue(value);
}

QString SystemSettings::defaultExportVideoCodec() const
{
    return d->defaultExportVideoCodecAdaptor->value();
}

void SystemSettings::setDefaultExportVideoCodec(const QString& value)
{
    d->defaultExportVideoCodecAdaptor->setValue(value);
}

QString SystemSettings::lowQualityScreenVideoCodec() const
{
    return d->lowQualityScreenVideoCodecAdaptor->value();
}

void SystemSettings::setLicenseServerUrl(const QString& value)
{
    d->licenseServerAdaptor->setValue(value);
}

QString SystemSettings::licenseServerUrl() const
{
    return d->licenseServerAdaptor->value();
}

nx::utils::Url SystemSettings::resourceFileUri() const
{
    return d->resourceFileUriAdaptor->value();
}

QString SystemSettings::cloudNotificationsLanguage() const
{
    return d->cloudNotificationsLanguageAdaptor->value();
}

void SystemSettings::setCloudNotificationsLanguage(const QString& value)
{
    d->cloudNotificationsLanguageAdaptor->setValue(value);
}

bool SystemSettings::keepIoPortStateIntactOnInitialization() const
{
    return d->keepIoPortStateIntactOnInitializationAdaptor->value();
}

void SystemSettings::setKeepIoPortStateIntactOnInitialization(bool value)
{
    d->keepIoPortStateIntactOnInitializationAdaptor->setValue(value);
}

int SystemSettings::mediaBufferSizeKb() const
{
    return d->mediaBufferSizeKbAdaptor->value();
}

void SystemSettings::setMediaBufferSizeKb(int value)
{
    d->mediaBufferSizeKbAdaptor->setValue(value);
}

int SystemSettings::mediaBufferSizeForAudioOnlyDeviceKb() const
{
    return d->mediaBufferSizeForAudioOnlyDeviceKbAdaptor->value();
}

void SystemSettings::setMediaBufferSizeForAudioOnlyDeviceKb(int value)
{
    d->mediaBufferSizeForAudioOnlyDeviceKbAdaptor->setValue(value);
}

bool SystemSettings::forceAnalyticsDbStoragePermissions() const
{
    return d->forceAnalyticsDbStoragePermissionsAdaptor->value();
}

std::chrono::milliseconds SystemSettings::checkVideoStreamPeriod() const
{
    return d->checkVideoStreamPeriodMsAdaptor->value();
}

void SystemSettings::setCheckVideoStreamPeriod(std::chrono::milliseconds value)
{
    d->checkVideoStreamPeriodMsAdaptor->setValue(value);
}

bool SystemSettings::useStorageEncryption() const
{
    return d->storageEncryptionAdaptor->value();
}

void SystemSettings::setUseStorageEncryption(bool value)
{
    d->storageEncryptionAdaptor->setValue(value);
}

QByteArray SystemSettings::currentStorageEncryptionKey() const
{
    return d->currentStorageEncryptionKeyAdaptor->value();
}

void SystemSettings::setCurrentStorageEncryptionKey(const QByteArray& value)
{
    d->currentStorageEncryptionKeyAdaptor->setValue(value);
}

bool SystemSettings::showServersInTreeForNonAdmins() const
{
    return d->showServersInTreeForNonAdminsAdaptor->value();
}

void SystemSettings::setShowServersInTreeForNonAdmins(bool value)
{
    d->showServersInTreeForNonAdminsAdaptor->setValue(value);
}

QString SystemSettings::additionalLocalFsTypes() const
{
    return d->additionalLocalFsTypesAdaptor->value();
}

void SystemSettings::setLowQualityScreenVideoCodec(const QString& value)
{
    d->lowQualityScreenVideoCodecAdaptor->setValue(value);
}

QString SystemSettings::forceLiveCacheForPrimaryStream() const
{
    return d->forceLiveCacheForPrimaryStreamAdaptor->value();
}

void SystemSettings::setForceLiveCacheForPrimaryStream(const QString& value)
{
    d->forceLiveCacheForPrimaryStreamAdaptor->setValue(value);
}

const QList<QnAbstractResourcePropertyAdaptor*>& SystemSettings::allSettings() const
{
    return d->allAdaptors;
}

const std::unordered_set<QString>& SystemSettings::allSettingNames() const
{
    return d->settingNames;
}

QList<const QnAbstractResourcePropertyAdaptor*> SystemSettings::allDefaultSettings() const
{
    QList<const QnAbstractResourcePropertyAdaptor*> result;
    for (const auto a: d->allAdaptors)
    {
        if (a->isDefault())
            result.push_back(a);
    }

    return result;
}

bool SystemSettings::isGlobalSetting(const nx::vms::api::ResourceParamWithRefData& param)
{
    return QnUserResource::kAdminGuid == param.resourceId
        && allSettingNames().contains(param.name);
}

nx::vms::api::MetadataStorageChangePolicy SystemSettings::metadataStorageChangePolicy() const
{
    return d->metadataStorageChangePolicyAdaptor->value();
}

void SystemSettings::setMetadataStorageChangePolicy(nx::vms::api::MetadataStorageChangePolicy value)
{
    d->metadataStorageChangePolicyAdaptor->setValue(value);
}

QString SystemSettings::serverHeader() const
{
    return d->serverHeaderAdaptor->value();
}

void SystemSettings::setServerHeader(const QString& value)
{
    d->serverHeaderAdaptor->setValue(value);
}

std::string SystemSettings::makeServerHeader() const
{
    return d->serverHeaderCache.get();
}

QString SystemSettings::supportedOrigins() const
{
    auto supportedOrigins = d->supportedOriginsAdaptor->value();
    if (supportedOrigins != "*" && !nx::network::SocketGlobals::cloud().cloudHost().empty())
    {
        auto cloudUrl = nx::utils::Url::fromUserInput(
            QString::fromStdString(nx::network::SocketGlobals::cloud().cloudHost()));
        cloudUrl.setScheme(nx::network::http::kSecureUrlSchemeName);

        if (supportedOrigins.isEmpty() || supportedOrigins == "null")
            supportedOrigins = cloudUrl.toString();
        else
            supportedOrigins += "," + cloudUrl.toString();
    }
    return supportedOrigins;
}
void SystemSettings::setSupportedOrigins(const QString& value)
{
    d->supportedOriginsAdaptor->setValue(value);
}

QString SystemSettings::frameOptionsHeader() const
{
    return d->frameOptionsHeaderAdaptor->value();
}
void SystemSettings::setFrameOptionsHeader(const QString& value)
{
    d->frameOptionsHeaderAdaptor->setValue(value);
}

bool SystemSettings::exposeServerEndpoints() const
{
    return d->exposeServerEndpointsAdaptor->value();
}

void SystemSettings::setExposeServerEndpoints(bool value)
{
    d->exposeServerEndpointsAdaptor->setValue(value);
}

bool SystemSettings::showMouseTimelinePreview() const
{
    return d->showMouseTimelinePreviewAdaptor->value();
}

void SystemSettings::setShowMouseTimelinePreview(bool value)
{
    d->showMouseTimelinePreviewAdaptor->setValue(value);
}

bool SystemSettings::system2faEnabled() const
{
    return d->system2faEnabledAdaptor->value();
}

void SystemSettings::setSystem2faEnabled(bool value)
{
    d->system2faEnabledAdaptor->setValue(value);
}

std::vector<nx::Uuid> SystemSettings::masterCloudSyncList() const
{
    std::vector<nx::Uuid> result;
    for (const auto& value: d->masterCloudSyncListAdaptor->value().split(";", Qt::SkipEmptyParts))
        result.push_back(nx::Uuid(value));
    return result;
}

void SystemSettings::setMasterCloudSyncList(const std::vector<nx::Uuid>& idList) const
{
    QStringList result;
    for (const auto& id: idList)
        result << id.toString();
    d->masterCloudSyncListAdaptor->setValue(result.join(";"));
}

std::map<QString, int> SystemSettings::specificFeatures() const
{
    return d->specificFeaturesAdaptor->value();
}

void SystemSettings::setSpecificFeatures(std::map<QString, int> value)
{
    d->specificFeaturesAdaptor->setValue(value);
}

bool SystemSettings::useHttpsOnlyForCameras() const
{
    return d->useHttpsOnlyForCamerasAdaptor->value();
}

void SystemSettings::setUseHttpsOnlyForCameras(bool value)
{
    d->useHttpsOnlyForCamerasAdaptor->setValue(value);
}

bool SystemSettings::securityForPowerUsers() const
{
    return d->securityForPowerUsersAdaptor->value();
}

void SystemSettings::setSecurityForPowerUsers(bool value)
{
    d->securityForPowerUsersAdaptor->setValue(value);
}

bool SystemSettings::isInsecureDeprecatedApiEnabled() const
{
    return d->insecureDeprecatedApiEnabledAdaptor->value();
}

void SystemSettings::enableInsecureDeprecatedApi(bool value)
{
    d->insecureDeprecatedApiEnabledAdaptor->setValue(value);
}

bool SystemSettings::isInsecureDeprecatedApiInUseEnabled() const
{
    return d->insecureDeprecatedApiInUseEnabledAdaptor->value();
}

void SystemSettings::enableInsecureDeprecatedApiInUse(bool value)
{
    d->insecureDeprecatedApiInUseEnabledAdaptor->setValue(value);
}

std::chrono::seconds SystemSettings::cloudPollingInterval() const
{
    return d->cloudPollingIntervalSAdaptor->value();
}

void SystemSettings::setCloudPollingInterval(std::chrono::seconds period)
{
    d->cloudPollingIntervalSAdaptor->setValue(period);
}

SystemSettings::Errors SystemSettings::update(const api::SaveableSystemSettings& value)
{
    Errors errors;
    #define VALUE(R, STRUCT, ITEM) \
        if (STRUCT.ITEM) \
        { \
            if (d->BOOST_PP_CAT(ITEM, Adaptor)->isValueValid(*STRUCT.ITEM)) \
                d->BOOST_PP_CAT(ITEM, Adaptor)->setValue(*STRUCT.ITEM); \
            else \
                errors.emplace(api::SystemSettingName::ITEM); \
        }
    BOOST_PP_SEQ_FOR_EACH(VALUE, value, SaveableSystemSettings_Fields)
    #undef VALUE
    return errors;
}

SystemSettings::Errors SystemSettings::update(const api::SystemSettings& value)
{
    const api::SaveableSystemSettings& base = value;
    auto errors = update(base);
    #define VALUE(R, STRUCT, ITEM) \
        if (d->BOOST_PP_CAT(ITEM, Adaptor)->isValueValid(STRUCT.ITEM)) \
            d->BOOST_PP_CAT(ITEM, Adaptor)->setValue(STRUCT.ITEM); \
        else \
            errors.emplace(api::SystemSettingName::ITEM);
    BOOST_PP_SEQ_FOR_EACH(VALUE, value, UnsaveableSystemSettings_Fields)
    #undef VALUE
    return errors;
}

api::SystemSettings SystemSettings::apiSettings() const
{
    api::SystemSettings result;
    #define VALUE(R, RESULT, ITEM) RESULT.ITEM = d->BOOST_PP_CAT(ITEM, Adaptor)->value();
    BOOST_PP_SEQ_FOR_EACH(VALUE, result, SystemSettings_Fields)
    #undef VALUE
    return result;
}

api::SystemSettings SystemSettings::apiSettings(const std::vector<api::SystemSettingName>& names) const
{
    api::SystemSettings result;
    for (auto name: names)
    {
        switch (name)
        {
            #define VALUE(R, RESULT, ITEM) \
                case api::SystemSettingName::ITEM: \
                    RESULT.ITEM = d->BOOST_PP_CAT(ITEM, Adaptor)->value(); \
                    break;
            BOOST_PP_SEQ_FOR_EACH(VALUE, result, SystemSettings_Fields)
            #undef VALUE
        }
    }
    return result;
}

} // namespace nx::vms::common
