#pragma once

namespace nx{
namespace plugins{
namespace flir{
namespace nexus{

const quint16 kDefaultNexusPort = 8080;

//This group of settings is from a private API. We shouldn't rely on it.
const QString kConfigurationFile("/api/server/status/full");
const QString kStartNexusServerCommand("/api/server/start");
const QString kStopNexusServerCommand("/api/server/stop");
const QString kServerSuccessfullyStarted("1");
const QString kServerSuccessfullyStopped("0");
const QString kNumberOfInputsParamName("Number of IOs");
const QString kNexusInterfaceGroupName("INTERFACE Configuration - Device 0");
const QString kNexusPortParamName("Port");

const QString kAlarmPrefix = lit("$ALARM");
const QString kThgSpotPrefix = lit("$THGSPOT");
const QString kThgAreaPrefix = lit("$THGAREA");
const QString kDigitalInputPrefix = lit("$DI");
const QString kDigitalOutputPrefix = lit("$DO");

const int kMdDeviceType = 12;
const int kIODeviceType = 28;
const int kThgObjectDeviceType = 5;
const int kThgSpotDeviceType = 53;
const int kThgAreaDeviceType = 54;

const QString kCommandPrefix = lit("Nexus.cgi");
const QString kControlPrefix = lit("NexusWS.cgi");
const QString kSubscriptionPrefix = lit("NexusWS_Status.cgi");
const QString kServerWhoAmICommand = lit("SERVERWhoAmI");
const QString kRequestControlCommand = lit("SERVERRemoteControlRequest");
const QString kReleaseControlCommand = lit("SERVERRemoteControlRelease");
const QString kSetOutputPortStateCommand = lit("IOSENSOROutputStateSet");

const QString kSessionParamName = lit("session");
const QString kSubscriptionNumParamName = lit("numSubscriptions");
const QString kNotificationFormatParamName = lit("NotificationFormat");
const QString kSubscriptionParamNamePrefix = lit("subscription");

const QString kJsonNotificationFormat = lit("JSON");
const QString kStringNotificationFormat = lit("String");

const int kNoError = 0;
const int kAnyDevice = -1;

const QString kAlarmSubscription = lit("ALARM");
const QString kThgSpotSubscription = lit("THGSPOT");
const QString kThgAreaSubscription = lit("THGAREA");
const QString kIOSubscription = lit("IO");

const QString kDateTimeFormat("yyyyMMddhhmmsszzz");

const QString kSessionIdParamName = lit("Id");

const QString kDiscoveryPrefix = lit("$NEXUS");
const QString kOldDiscoveryPrefix = lit("$LUVEO");
const int kDiscoveryMessageFieldsNumber = 13;

using NexusSettingGroup = std::map<QString, QString>;

struct ServerStatus final
{
    std::map<QString, NexusSettingGroup> settings;
    bool isNexusServerEnabled = true;
};

struct Notification final
{
    QString alarmId;
    int alarmState = 0;
};

struct Subscription final
{
    Subscription() = default;
    Subscription(const QString& subscriptionTypeString):
        subscriptionType(subscriptionTypeString)
    {
    };

    QString subscriptionType;
    int deviceId = kAnyDevice;
    std::chrono::milliseconds minDeliveryInterval = std::chrono::milliseconds(1000);
    std::chrono::milliseconds maxDeliveryInterval = std::chrono::milliseconds(1000);
    bool onChange = false;
};

struct PrivateDeviceInfo final
{
    QString model;
    QString serialNumber;
    QUrl url;
};

enum class SensorType
{
    shortRange = 1,
    midRange,
    longRange,
    wideEye,
    foveus,
    radar,
    cctv,
    uav
};

enum class HostType
{
    windows = 1,
    compactServer,
    miniServer,
    sentirServer,
    cieloBoard
};

enum class TransmissionType
{
    unicast = 1,
    multicast
};

struct DeviceDiscoveryInfo final
{
    QString serverName;
    QString serverId;
    QString ipAddress;
    uint tcpPort;
    TransmissionType transmissionType;
    QString multicastAddress;
    uint multicastPort;
    int ttl;
    SensorType sensorType;
    int nmeaInterval;
    int timeout;
    HostType hostType;
};

} //namespace nexus
} //namespace flir
} //namespace plugins
} //namespace nx