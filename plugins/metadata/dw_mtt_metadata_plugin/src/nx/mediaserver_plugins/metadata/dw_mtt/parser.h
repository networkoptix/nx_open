#include <QString>
#include <QDomDocument>
#include <QFlags>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

enum EventState
{
    noneEvent = 0b0001,
    startEvent = 0b0010,
    stopEvent = 0b0100,
    procedureEvent = 0b1000
};
Q_DECLARE_FLAGS(EventStatus, EventState)

QDomElement findChildElementByTag(const QDomNode& node, const QString& tag);

QDomElement findNestedChildElement(const QDomNode& node, const QString& tags);

EventStatus readEventStatus(const QDomNode& node);

bool isRatioMoreThenTreshold(const QDomNode& node);

QByteArray getEventName(const QDomDocument& dom);

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
