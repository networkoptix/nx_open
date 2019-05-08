#pragma once

#include <chrono>

#include <QtCore/QString>

namespace nx {
namespace plugins {
namespace flir {
namespace nexus {

const quint16 kDefaultNexusPort = 8090;

const QString kAlarmPrefix = "$ALARM";
const QString kThgSpotPrefix = "$THGSPOT";
const QString kThgAreaPrefix = "$THGAREA";
const QString kMdAreaPrefix = "$MD";
const QString kDigitalInputPrefix = "$DI";
const QString kDigitalOutputPrefix = "$DO";

const int kMdDeviceType = 12;
const int kIODeviceType = 28;
const int kThgObjectDeviceType = 5;
const int kThgSpotDeviceType = 53;
const int kThgAreaDeviceType = 54;

const QString kCommandPrefix = "Nexus.cgi";
const QString kControlPrefix = "NexusWS.cgi";
const QString kSubscriptionPrefix = "NexusWS_Status.cgi";
const QString kServerWhoAmICommand = "SERVERWhoAmI";
const QString kRequestControlCommand = "SERVERRemoteControlRequest";
const QString kRequestControlAsyncCommand = "SERVERRemoteControlRequestAsync";
const QString kReleaseControlCommand = "SERVERRemoteControlRelease";
const QString kSetOutputPortStateCommand = "IOSENSOROutputStateSet";

const QString kSessionParamName = "session";
const QString kSubscriptionNumParamName = "numSubscriptions";
const QString kNotificationFormatParamName = "NotificationFormat";
const QString kSubscriptionParamNamePrefix = "subscription";

const QString kJsonNotificationFormat = "JSON";
const QString kStringNotificationFormat = "String";

const int kNoError = 0;
const int kAnyDevice = -1;

const QString kAlarmSubscription = "ALARM";
const QString kThgSpotSubscription = "THGSPOT";
const QString kThgAreaSubscription = "THGAREA";
const QString kIOSubscription = "IO";

const QString kDateTimeFormat("yyyyMMddhhmmsszzz");

const QString kSessionIdParamName = "Id";

const QString kDiscoveryPrefix = "$NEXUS";
const QString kOldDiscoveryPrefix = "$LUVEO";
const int kDiscoveryMessageFieldsNumber = 13;

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
    }

    QString toString() const
    {
        return QStringLiteral("\"%1,%2,%3,%4,%5\"")
            .arg(subscriptionType)
            .arg(deviceId)
            .arg(minDeliveryInterval.count())
            .arg(maxDeliveryInterval.count())
            .arg(onChange);
    }

    QString subscriptionType;
    int deviceId = kAnyDevice;
    std::chrono::milliseconds minDeliveryInterval = std::chrono::milliseconds(1000);
    std::chrono::milliseconds maxDeliveryInterval = std::chrono::milliseconds(1000);
    bool onChange = false;
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

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx
