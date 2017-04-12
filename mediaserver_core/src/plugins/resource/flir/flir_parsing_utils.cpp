#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include "flir_parsing_utils.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

using namespace nx::plugins::flir;

namespace {

const QString kProductInfoKey = lit("product");
const QString kModelNumberKey = lit("modelNumber");
const QString kSerialNumberKey = lit("serialNumber");

bool isThgObjectNotificationType(const QString& notificationTypeString)
{
    return notificationTypeString == nexus::kThgSpotPrefix 
        || notificationTypeString == nexus::kThgAreaPrefix;
};

boost::optional<nexus::Notification> parseAlarmNotification(const QStringList& notificationParts)
{
    nexus::Notification alarmEvent;
    bool status = false;
    const auto kDeviceTypeFieldPosition = 5;
    const auto kAlarmSourceIndexFieldPosition = 6;
    const auto kAlarmStateFieldPosition = 9;

    const auto kDeviceType = 
        notificationParts[kDeviceTypeFieldPosition].toInt(&status);

    if (!status)
        return boost::none;

    const auto kAlarmSourceIndex = 
        notificationParts[kAlarmSourceIndexFieldPosition].toInt(&status);

    if (!status)
        return boost::none;

    QString prefix;
    if (kDeviceType == nexus::kIODeviceType)
        prefix = nexus::kDigitalInputPrefix;
    else if (kDeviceType == nexus::kThgObjectDeviceType)
        prefix = nexus::kAlarmPrefix;
    else if (kDeviceType == nexus::kMdDeviceType)
        prefix = nexus::kMdAreaPrefix;

    alarmEvent.alarmId = lit("%1:%2")
        .arg(prefix)
        .arg(kAlarmSourceIndex);

    alarmEvent.alarmState = notificationParts[kAlarmStateFieldPosition].toInt(&status);

    if (!status)
        return boost::none;

    return alarmEvent;
};

boost::optional<nexus::Notification> parseThgObjectNotification(const QStringList& notificationParts)
{
    nexus::Notification alarmEvent;
    bool status = false;
    const auto kObjectTypeFieldPosition = 0;
    const auto kObjectIndexFieldPosition = 7;
    const auto kObjectStateFieldPosition = 11;

    const auto kObjectType = notificationParts[kObjectTypeFieldPosition];
    const auto kObjectIndex = notificationParts[kObjectIndexFieldPosition].toInt(&status);

    if (!status)
        return boost::none;

    NX_ASSERT(
        isThgObjectNotificationType(kObjectType),
        lm("Flir notification parser: wrong notification type %1. %2 or %3 are expected.")
            .arg(kObjectType)
            .arg(nexus::kThgAreaPrefix)
            .arg(nexus::kThgSpotPrefix));

    if (!isThgObjectNotificationType(kObjectType))
        return boost::none;

    alarmEvent.alarmId = lit("%1:%2")
        .arg(kObjectType)
        .arg(kObjectIndex);

    alarmEvent.alarmState = notificationParts[kObjectStateFieldPosition].toInt(&status);

    if (!status)
        return boost::none;

    return alarmEvent;
};

bool isValidTransmissionType(int rawValue)
{
    return rawValue == static_cast<int>(nexus::TransmissionType::unicast) 
        || rawValue == static_cast<int>(nexus::TransmissionType::multicast);
};

bool isValidHostType(int rawValue)
{
    return rawValue == static_cast<int>(nexus::HostType::windows)
        || rawValue == static_cast<int>(nexus::HostType::miniServer)
        || rawValue == static_cast<int>(nexus::HostType::compactServer)
        || rawValue == static_cast<int>(nexus::HostType::sentirServer)
        || rawValue == static_cast<int>(nexus::HostType::cieloBoard);
};

bool isValidSensortype(int rawValue)
{
    return rawValue == static_cast<int>(nexus::SensorType::shortRange)
        || rawValue == static_cast<int>(nexus::SensorType::midRange)
        || rawValue == static_cast<int>(nexus::SensorType::longRange)
        || rawValue == static_cast<int>(nexus::SensorType::wideEye)
        || rawValue == static_cast<int>(nexus::SensorType::radar)
        || rawValue == static_cast<int>(nexus::SensorType::uav)
        || rawValue == static_cast<int>(nexus::SensorType::cctv)
        || rawValue == static_cast<int>(nexus::SensorType::foveus);
};

} // namespace

namespace nx {
namespace plugins {
namespace flir {

boost::optional<nexus::Notification> parseNotification(
    const QString& notificationString)
{
    boost::optional<nexus::Notification> notification;

    const auto kParts = notificationString.split(L',');
    const auto kNotificationType = kParts[0];

    if (kNotificationType == nexus::kAlarmPrefix)
        notification = parseAlarmNotification(kParts);
    else if (isThgObjectNotificationType(kNotificationType))
        notification = parseThgObjectNotification(kParts);
    else
        return boost::none;

    return notification;
}

boost::optional<quint64> parseNotificationDateTime(const QString& dateTimeString)
{
    auto dateTime = QDateTime::fromString(dateTimeString, nexus::kDateTimeFormat);

    if (!dateTime.isValid())
        return boost::none;

    return dateTime.toMSecsSinceEpoch();
}

boost::optional<nexus::Subscription> parseSubscription(const QString& subscriptionString)
{

    nexus::Subscription subscription;
    const int kSubscriptionPartsNumber = 5;
    bool status = false;
    auto subscriptionParts = subscriptionString.split(L',');

    if (subscriptionParts.size() != kSubscriptionPartsNumber)
        return boost::none;

    subscription.subscriptionType = subscriptionParts.at(0);
    subscription.deviceId = subscriptionParts.at(1).toInt(&status);

    if (!status)
        return boost::none;

    int deliveryInterval = subscriptionParts.at(2).toInt(&status);

    if (!status)
        return boost::none;

    subscription.minDeliveryInterval = std::chrono::milliseconds(deliveryInterval);

    deliveryInterval = subscriptionParts.at(3).toInt(&status);

    if (!status)
        return boost::none;

    subscription.maxDeliveryInterval = std::chrono::milliseconds(deliveryInterval);

    bool onChange = subscriptionParts.at(4).toInt(&status);

    if (!status)
        return boost::none;

    subscription.onChange = onChange;

    return subscription;
}

boost::optional<fc_private::ServerStatus> parseNexusServerStatusResponse(const QString& response)
{
    fc_private::ServerStatus serverStatus;
    QString currentGroupName;

    bool status = false;
    auto lines = response.split(lit("\n"));

    if (lines.isEmpty())
        return boost::none;

    serverStatus.isNexusServerEnabled = lines.back().trimmed().toInt(&status);
    if (!status)
        return boost::none;

    lines.pop_back();

    for (const auto& line : lines)
    {
        auto trimmedLine = line.trimmed();
        if (trimmedLine.startsWith(L'[') && trimmedLine.size() > 3)
        {
            // Group name pattern is [Group Name]
            currentGroupName = trimmedLine
                .mid(1, trimmedLine.size() - 2)
                .trimmed();
        }
        else
        {
            auto settingNameAndValue = trimmedLine.split("=");
            auto partsCount = settingNameAndValue.size();
            auto& currentGroup = serverStatus.settings[currentGroupName];

            if (partsCount == 2)
                currentGroup[settingNameAndValue[0]] = settingNameAndValue[1];
            else if (partsCount == 1)
                currentGroup[settingNameAndValue[0]] = QString();
        }
    }

    return serverStatus;
}

boost::optional<fc_private::DeviceInfo> parsePrivateDeviceInfo(const QString& deviceInfoString)
{
    fc_private::DeviceInfo info;

    auto jsonDocument = QJsonDocument::fromJson(deviceInfoString.toUtf8());
    if (!jsonDocument.isObject())
        return boost::none;

    auto jsonObject = jsonDocument.object();

    if (!jsonObject.contains(kProductInfoKey) || !jsonObject[kProductInfoKey].isObject())
        return boost::none;

    auto productObject = jsonObject[kProductInfoKey].toObject();

    if (!productObject.contains(kModelNumberKey) || !productObject[kModelNumberKey].isString())
        return boost::none;

    info.model = productObject[kModelNumberKey].toString();

    if (!productObject.contains(kSerialNumberKey) || !productObject[kSerialNumberKey].isString())
        return boost::none;

    info.serialNumber = productObject[kSerialNumberKey].toString();

    return info;
}

boost::optional<nexus::DeviceDiscoveryInfo> parseDeviceDiscoveryInfo(const QString& deviceDiscoveryInfoString)
{
    nexus::DeviceDiscoveryInfo info;
    bool status = false;
    auto parts = deviceDiscoveryInfoString.split(L',');

    if (parts.size() != nexus::kDiscoveryMessageFieldsNumber)
        return boost::none;

    if (parts[0] != nexus::kDiscoveryPrefix && parts[0] != nexus::kOldDiscoveryPrefix)
        return boost::none;

    info.serverName = parts[1];
    info.serverId = parts[2];
    info.ipAddress = parts[3];
    info.tcpPort = parts[4].toUInt(&status);
    if (!status)
        return boost::none;

    auto transmissionType = parts[5].toInt(&status);
    if (!status || !isValidTransmissionType(transmissionType))
        return boost::none;

    info.transmissionType = static_cast<nexus::TransmissionType>(transmissionType);
    info.multicastAddress = parts[6];
    info.multicastPort = parts[7].toUInt(&status);
    if (!status)
        return boost::none;

    info.ttl = parts[8].toUInt(&status);
    if (!status)
        return boost::none;

    auto sensorType = parts[9].toUInt(&status);

    if (!status || !isValidSensortype(sensorType))
        return boost::none;

    info.sensorType = static_cast<nexus::SensorType>(sensorType);
    info.nmeaInterval = parts[10].toUInt(&status);
    if (!status)
        return boost::none;

    info.timeout = parts[11].toUInt(&status);
    if (!status)
        return boost::none;

    auto hostType = parts[12].toInt(&status);

    if (!status || !isValidHostType(hostType))
        return boost::none;

    return info;
}

} // namespace flir
} // namespace plugins
} // namespace nx


