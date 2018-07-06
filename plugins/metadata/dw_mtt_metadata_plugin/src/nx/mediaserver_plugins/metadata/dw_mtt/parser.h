#include <QString>
#include <QDomDocument>

struct StartStopStatus
{
    bool start;
    bool stop;
};

QDomElement findChildElementByTag(const QDomNode& node, const QString& tag);

QDomElement findNestedChildElement(const QDomNode& node, const QString& tags);

StartStopStatus readEventStatus(const QDomNode& node);

bool ratioExceedsThreshold(const QDomNode& node);

QByteArray getEventName(const QDomDocument& dom);
