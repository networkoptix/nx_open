#include "flir_nexus_parsing_utils.h"
#include "flir_nexus_common.h"

using namespace nx::plugins::flir::nexus;

namespace{

bool isThgObjectNotificationType(const QString& notificationTypeString)
{
    return notificationTypeString == kThgSpotPrefix || notificationTypeString == kThgAreaPrefix;
};

Notification parseAlarmNotification(
    const QStringList& notificationParts,
    bool* outStatus)
{
    Notification alarmEvent;

    const auto kDeviceTypeFieldPosition = 5;
    const auto kAlarmSourceIndexFieldPosition = 6;
    const auto kAlarmStateFieldPosition = 9;

    const auto kDeviceType = notificationParts[kDeviceTypeFieldPosition]
        .toInt(outStatus);

    if (!*outStatus)
        return alarmEvent;

    const auto kAlarmSourceIndex = notificationParts[kAlarmSourceIndexFieldPosition]
        .toInt(outStatus);

    if (!*outStatus)
        return alarmEvent;

    QString prefix;
    if (kDeviceType == kIODeviceType)
        prefix = kDigitalInputPrefix;
    else if (kDeviceType == kThgObjectDeviceType)
        prefix = kAlarmPrefix;


    alarmEvent.alarmId = lit("%1:%2")
        .arg(prefix)
        .arg(kAlarmSourceIndex);

    alarmEvent.alarmState = notificationParts[kAlarmStateFieldPosition].toInt(outStatus);

    return alarmEvent;
};

Notification parseThgObjectNotification(
    const QStringList& notificationParts,
    bool* outStatus)
{
    Notification alarmEvent;

    const auto kObjectTypeFieldPosition = 0;
    const auto kObjectIndexFieldPosition = 7;
    const auto kObjectStateFieldPosition = 11;

    const auto kObjectType = notificationParts[kObjectTypeFieldPosition];
    const auto kObjectIndex = notificationParts[kObjectIndexFieldPosition].toInt(outStatus);

    if (!*outStatus)
        return alarmEvent;

    NX_ASSERT(
        isThgObjectNotificationType(kObjectType),
        lm("Flir notification parser: wrong notification type %1. %2 or %3 are expected.")
        .arg(kObjectType)
        .arg(kThgAreaPrefix)
        .arg(kThgSpotPrefix));

    if (!isThgObjectNotificationType(kObjectType))
    {
        *outStatus = false;
        return alarmEvent;
    }

    alarmEvent.alarmId = lit("%1:%2")
        .arg(kObjectType)
        .arg(kObjectIndex);

    alarmEvent.alarmState = notificationParts[kObjectStateFieldPosition].toInt(outStatus);

    return alarmEvent;
};

} // namespace

namespace nx{
namespace plugins{
namespace flir{
namespace nexus{

Notification parseNotification(
    const QString& message,
    bool* outStatus)
{
    Notification notification;

    const auto kParts = message.split(L',');
    const auto kNotificationType = kParts[0];

    if (kNotificationType == kAlarmPrefix)
        notification = parseAlarmNotification(kParts, outStatus);
    else if (isThgObjectNotificationType(kNotificationType))
        notification = parseThgObjectNotification(kParts, outStatus);
    else
        *outStatus = false;

    return notification;
}

quint64 parseNotificationDateTime(const QString& dateTimeString, bool* outStatus)
{
    *outStatus = true;
    auto dateTime = QDateTime::fromString(dateTimeString, nexus::kDateTimeFormat);

    if (!dateTime.isValid())
        outStatus = false;

    return dateTime.toMSecsSinceEpoch();
}

NexusServerStatus parseNexusServerStatusResponse(const QString& response)
{
    qDebug() << "Parsing Nexus server status response:" << response;
    NexusServerStatus serverStatus;
    QString currentGroupName;
    auto lines = response.split(lit("\n"));

    if (lines.isEmpty())
        return serverStatus;

    serverStatus.isNexusServerEnabled = lines.back().trimmed().toInt();
    lines.pop_back();

    for (const auto& line : lines)
    {
        auto trimmedLine = line.trimmed();
        if (trimmedLine.startsWith(L'['))
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

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx
