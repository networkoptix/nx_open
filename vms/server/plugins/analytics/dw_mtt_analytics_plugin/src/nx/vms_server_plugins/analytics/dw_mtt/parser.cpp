#include "parser.h"

#include <QStringList>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dw_mtt {

QList<AlarmPair> getAlarmPairs(const QDomDocument& dom)
{
    /*
    Incomming xml has the following format:
    <config version="1.0" xmlns="http://www.ipc.com/ver10">
        <alarmStatusInfo>
            <alarmTag1 type="boolean" id="1">false</alarmTag1>
            <alarmTag2 type="boolean" id="1">false</alarmTag2>
            ...
    */
    QList<AlarmPair> result;

    for (QDomNode alarm = dom.firstChild().firstChild().firstChild();
        !alarm.isNull();
        alarm = alarm.nextSibling())
    {
        const QString alarmTag = alarm.nodeName();
        const QString alarmValue = alarm.toElement().text();
        const bool value = (alarmValue == "true");
        result.push_back(AlarmPair{ alarmTag.toUtf8(), value });
    }

    return result;
}

} // namespace dw_mtt
} // namespace analytics
} // namespace mediaserver_plugin
} // namespace nx
