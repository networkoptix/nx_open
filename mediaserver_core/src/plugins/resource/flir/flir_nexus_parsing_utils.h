#pragma once

namespace nx {
namespace plugins{
namespace flir{
namespace nexus{

using NexusSettingGroup = std::map<QString, QString>;

struct NexusServerStatus
{
    std::map<QString, NexusSettingGroup> settings;
    bool isNexusServerEnabled = true;
};

struct Notification
{
    QString alarmId;
    int alarmState = 0;
};

Notification parseNotification(const QString& notificationString, bool *outStatus);

quint64 parseNotificationDateTime(const QString& dateTimeString, bool* outStatus);

NexusServerStatus parseNexusServerStatusResponse(const QString& response);

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx