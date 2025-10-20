// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <api/model/api_ioport_data.h>
#include <api/model/configure_system_data.h>
#include <api/resource_property_adaptor.h>
#include <core/resource/resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/reflect/string_conversion.h>
#include <nx/vms/api/data/system_setting_description.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/api/types/device_type.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>

namespace nx::vms::api::test {

TEST(ConfigureSystemData, deserialization)
{
    ConfigureSystemData data1;

    data1.localSystemId = nx::Uuid::createUuid();
    data1.wholeSystem = true;
    data1.sysIdTime = 12345;
    data1.tranLogTime = nx::vms::api::Timestamp(1, 2);
    data1.port = 7070;
    data1.foreignServer.name = "server1";
    UserData user1;
    user1.name = "user1";
    data1.foreignUsers.push_back(user1);
    nx::vms::api::ResourceParamData setting1("key1", "value1");
    data1.foreignSettings.push_back(setting1);
    data1.rewriteLocalSettings = true;
    data1.systemName = "system1";
    data1.currentPassword = "123";

    auto serializedData = QJson::serialized(data1);
    auto data2 = QJson::deserialized<ConfigureSystemData>(serializedData);

    ASSERT_EQ(data1, data2);
}

TEST(DeviceType, serialization)
{
    using namespace nx::vms::api;

    ASSERT_EQ("Camera", nx::reflect::toString(DeviceType::camera));
    ASSERT_EQ(DeviceType::camera, nx::reflect::fromString<DeviceType>("Camera"));
    ASSERT_EQ(DeviceType::camera, nx::reflect::fromString<DeviceType>("1"));
}

TEST(IOPortType, serialization)
{
    QByteArray result;
    Qn::IOPortType data = Qn::PT_Input;
    QJson::serialize(data, &result);
    ASSERT_EQ(result, "\"Input\"");
}

TEST(IOPortTypes, serialization)
{
    QByteArray result;
    Qn::IOPortTypes data = Qn::PT_Input | Qn::PT_Output;
    QJson::serialize(data, &result);
    ASSERT_EQ(result, "\"Input|Output\"");
}

namespace {

const SettingsBase kDefaultSettingsBase{
    .cloudAccountName = QString{},
    .cloudHost = QString{},
    .lastMergeMasterId = QString{},
    .lastMergeSlaveId = QString{},
    .organizationId = nx::Uuid{},
    .specificFeatures = std::map<QString, int>{},
    .statisticsReportLastNumber = 0,
    .statisticsReportLastTime = QString{},
    .statisticsReportLastVersion = QString{}
};

const SaveableSettingsBase kDefaultSaveableSettingsBase{
    .defaultExportVideoCodec = QString{"mpeg4"},
    .watermarkSettings = WatermarkSettings{},
    .pixelationSettings = PixelationSettings{},
    .webSocketEnabled = true,

    .autoDiscoveryEnabled = true,
    .cameraSettingsOptimization = true,
    .statisticsAllowed = true,
    .defaultUserLocale = QString{},

    .auditTrailEnabled = true,
    .trafficEncryptionForced = true,
    .useHttpsOnlyForCameras = false,
    .videoTrafficEncryptionForced = false,
    .storageEncryption = false,
    .showServersInTreeForNonAdmins = true,

    .updateNotificationsEnabled = true,

    .emailSettings = EmailSettings{
        .useCloudServiceToSendEmail = false
    },

    .timeSynchronizationEnabled = true,
    .primaryTimeServer = nx::Uuid{},
    .customReleaseListUrl = nx::Url{},

    .clientUpdateSettings = ClientUpdateSettings{},
    .backupSettings = BackupSettings{},
    .metadataStorageChangePolicy = MetadataStorageChangePolicy::keep,

    .allowRegisteringIntegrations = false,

    .additionalLocalFsTypes = QString{},
    .arecontRtspEnabled = false,
    .auditTrailPeriodDays = 183,
    .autoDiscoveryResponseEnabled = true,
    .autoUpdateThumbnails = true,
    .checkVideoStreamPeriodMs = 10s,
    .clientStatisticsSettingsUrl = QString{},
    .cloudConnectRelayingEnabled = true,
    .cloudConnectRelayingOverSslForced = false,
    .cloudConnectUdpHolePunchingEnabled = true,
    .cloudPollingIntervalS = 1min,
    .crashReportServerApi = QString{},
    .crossdomainEnabled = false,
    .currentStorageEncryptionKey = QByteArray{},
    .defaultVideoCodec = QString{"h263p"},
    .deviceStorageInfoUpdateIntervalS = 604800s,
    .disabledVendors = QString{},
    .downloaderPeers = QMap<QString, QList<nx::Uuid>>{},
    .ec2AliveUpdateIntervalSec = 60,
    .enableEdgeRecording = true,
    .eventLogPeriodDays = 30,
    .exposeDeviceCredentials = true,
    .exposeServerEndpoints = true,
    .forceAnalyticsDbStoragePermissions = true,
    .forceLiveCacheForPrimaryStream = QString{"auto"},
    .frameOptionsHeader = QString{"SAMEORIGIN"},
    .insecureDeprecatedApiEnabled = false,
    .insecureDeprecatedApiInUseEnabled = true,
    .insecureDeprecatedAuthEnabled = true,
    .installedPersistentUpdateStorage = PersistentUpdateStorage{},
    .installedUpdateInformation = QByteArray{},
    .keepIoPortStateIntactOnInitialization = false,
    .licenseServer = QString{"https://licensing.vmsproxy.com"},
    .lowQualityScreenVideoCodec = QString{"mpeg2video"},
    .masterCloudSyncList = QString{},
    .maxBookmarks = 0,
    .maxDifferenceBetweenSynchronizedAndInternetTime = 2'000,
    .maxDifferenceBetweenSynchronizedAndLocalTimeMs = 2s,
    .maxEventLogRecords = 100'000,
    .maxHttpTranscodingSessions = 8,
    .maxP2pAllClientsSizeBytes = 1024 * 1024 * 128,
    .maxP2pQueueSizeBytes = 1024 * 1024 * 128,
    .maxRecordQueueSizeBytes = 1024 * 1024 * 24,
    .maxRecordQueueSizeElements = 1'000,
    .maxRemoteArchiveSynchronizationThreads = -1,
    .maxRtpRetryCount = 6,
    .maxRtspConnectDurationSeconds = 0,
    .maxSceneItems = 0,
    .maxVirtualCameraArchiveSynchronizationThreads = -1,
    .mediaBufferSizeForAudioOnlyDeviceKb = 16,
    .mediaBufferSizeKb = 256,
    .osTimeChangeCheckPeriodMs = 5s,
    .proxyConnectTimeoutSec = 5,
    .proxyConnectionAccessPolicy = ProxyConnectionAccessPolicy::powerUsers,
    .resourceFileUri = nx::Url{"https://resources.vmsproxy.com/resource_data.json"},
    .rtpTimeoutMs = 10s,
    .securityForPowerUsers = true,
    .sequentialFlirOnvifSearcherEnabled = false,
    .serverHeader = QString{"$vmsName/$vmsVersion ($company) $compatibility"},
    .showMouseTimelinePreview = true,
    .statisticsReportServerApi = QString{},
    .statisticsReportTimeCycle = QString{"30d"},
    .statisticsReportUpdateDelay = QString{"3h"},
    .supportedOrigins = QString{"*"},
    .syncTimeEpsilon = 200,
    .syncTimeExchangePeriod = 600'000,
    .targetPersistentUpdateStorage = PersistentUpdateStorage{},
    .targetUpdateInformation = QByteArray{},
    .upnpPortMappingEnabled = true,
    .useTextEmailFormat = false,
    .useWindowsEmailLineFeed = false
};

const UserSessionSettings kDefaultUserSessionSettings{
    .sessionLimitS = std::chrono::days{30},
    .sessionsLimit = 100'000,
    .sessionsLimitPerUser = 5'000,
    .remoteSessionTimeoutS = 6h,
    .remoteSessionUpdateS = 10s,
    .remoteSessionUpdateKeysS = 5min,
    .useSessionLimitForCloud = false
};

const SaveableSystemSettings kDefaultSaveableSystemSettings{
    kDefaultSaveableSettingsBase,
    kDefaultUserSessionSettings,
    /*systemName*/ QString{}};

enum SystemSettingKind:bool {notReadOnly, notWriteOnly};

void checkSystemSettings(
    const QList<QnAbstractResourcePropertyAdaptor*>& adaptors, SystemSettingKind kind)
{
    QnJsonContext context;
    context.setChronoSerializedAsDouble(true);
    SystemSettingsValues values;
    for (const auto& a: adaptors)
    {
        switch (kind)
        {
            case notReadOnly:
                if (!a->isReadOnly())
                    values.emplace(a->key(), a->jsonValue(&context));
                break;
            case notWriteOnly:
                if (!a->isWriteOnly())
                    values.emplace(a->key(), a->jsonValue(&context));
                break;
        }
    }
    auto expected = nx::utils::formatJsonString(QJson::serialized(values));
    QByteArray qjson;
    std::string reflect;
    switch (kind)
    {
        case notReadOnly:
            QJson::serialize(&context, kDefaultSaveableSystemSettings, &qjson);
            reflect = nx::reflect::json::serialize(kDefaultSaveableSystemSettings);
            break;
        case notWriteOnly:
        {
            SystemSettings defaultSettings;
            static_cast<SaveableSystemSettings&>(defaultSettings) = kDefaultSaveableSystemSettings;
            static_cast<SettingsBase&>(defaultSettings) = kDefaultSettingsBase;
            defaultSettings.cloudSystemID = QString{};
            defaultSettings.localSystemId = nx::Uuid{};
            defaultSettings.system2faEnabled = false;
            QJson::serialize(&context, defaultSettings, &qjson);
            reflect = nx::reflect::json::serialize(defaultSettings);
            break;
        }
    }
    qjson = nx::utils::formatJsonString(qjson);
    ASSERT_EQ(expected.toStdString(), qjson.toStdString());
    QJsonValue valueFromReflect;
    ASSERT_TRUE(QJson::deserialize(reflect, &valueFromReflect));
    QByteArray qjsonFromReflect;
    QJson::serialize(valueFromReflect, &qjsonFromReflect);
    qjsonFromReflect = nx::utils::formatJsonString(qjsonFromReflect);
    ASSERT_EQ(expected.toStdString(), qjsonFromReflect.toStdString());
}

struct TestProperty
{
    std::string name;
    std::optional<std::string> password;
    int timeout = 60;

    bool operator==(const TestProperty& other) const = default;

    friend void PrintTo(const TestProperty& val, ::std::ostream* os)
    {
        *os << QJson::serialized(val).toStdString();
    }
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TestProperty, (json), (name)(password)(timeout))

struct TestResource: QnResource
{
    TestResource()
    {
        setForceUsingLocalProperties(true);
    }

    void setTestProperty(TestProperty value)
    {
        setTestProperty(QString::fromUtf8(QJson::serialized(value)));
    }

    void setTestProperty(QString data)
    {
        setProperty("testProperty", data);
        emitPropertyChanged("testProperty", "", data);
    }
};

} // namespace

TEST(SystemSettings, serialization)
{
    common::SystemContext systemContext{
        common::SystemContext::Mode::unitTests, nx::Uuid::createUuid()};
    common::SystemSettings systemSettings{&systemContext};
    const auto adaptors = systemSettings.allSettings();
    checkSystemSettings(adaptors, notReadOnly);
    checkSystemSettings(adaptors, notWriteOnly);
}

TEST(SystemSettings, settingsWithOptionalUpdate)
{
    QnSharedResourcePointer<TestResource> resource(new TestResource());
    QnJsonResourcePropertyAdaptor<TestProperty> adapter("testProperty", TestProperty{});
    adapter.setResource(resource);
    EXPECT_EQ(adapter.value(), TestProperty{});
    const std::vector<TestProperty> testValues = {
        {.name = "name1", .password = std::nullopt},
        {.name = "name2", .password = "password2"},
        {.name = "", .password = std::nullopt},
        {.name = "name3", .password = std::nullopt, .timeout = 10},
    };
    NX_INFO(this, "Update adapter from property");
    for (const auto& testValue: testValues)
    {
        resource->setTestProperty(testValue);
        EXPECT_EQ(adapter.value(), testValue);
    }

    NX_INFO(this, "Update adapter directly");
    for (const auto& testValue: testValues)
    {
        adapter.setValue(testValue);
        EXPECT_EQ(adapter.value(), testValue);
    }
    resource->setTestProperty(QString());
    EXPECT_EQ(adapter.value(), TestProperty{});
}

} // namespace nx::vms::api::test
