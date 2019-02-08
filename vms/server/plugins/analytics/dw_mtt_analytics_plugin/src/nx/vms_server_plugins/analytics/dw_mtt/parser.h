#pragma once

#include <QString>
#include <QDomDocument>
#include <QFlags>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dw_mtt {

enum class EventState
{
    noneEvent = 1 << 0,
    startEvent = 1 << 1,
    stopEvent = 1 << 2,
    procedureEvent = 1 << 3,
};
Q_DECLARE_FLAGS(EventStatus, EventState)

QDomElement findChildElementByTag(const QDomNode& node, const QString& tag);

QDomElement findNestedChildElement(const QDomNode& node, const QString& tags);

EventStatus readEventStatus(const QDomNode& node);

bool isRatioMoreThenTreshold(const QDomNode& node);

QByteArray getEventName(const QDomDocument& dom);

struct AlarmPair
{
    QByteArray alarmName;
    bool value = false;
};

QList<AlarmPair> getAlarmPairs(const QDomDocument& dom);

} // namespace dw_mtt
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
