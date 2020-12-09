#pragma once

#include <QString>
#include <QDomDocument>
#include <QFlags>

namespace nx::vms_server_plugins::analytics::dw_mx9 {

struct AlarmPair
{
    QByteArray alarmName;
    bool value = false;
};

QList<AlarmPair> getAlarmPairs(const QDomDocument& dom);

} // namespace nx::vms_server_plugins::analytics::dw_mx9
