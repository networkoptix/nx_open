#include "parser.h"

#include <QStringList>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

/**
* Scans all children of a node 'node' and finds an element with tag 'tag'.
* If no such a child returns null element.
*/
QDomElement findChildElementByTag(const QDomNode& node, const QString& tag)
{
    QDomElement element;
    for (QDomNode child = node.firstChild(); !child.isNull(); child = child.nextSibling())
    {
        const QString childTag = child.nodeName();
        if (childTag == tag)
        {
            element = child.toElement();
            break;
        }
    }
    return element;
}

/**
* Returns an element with path 'tags' in the node 'node'.
* The example of 'tags': "tripwire/tripInfo/item/status".
* If no such a child returns null element.
*/
QDomElement findNestedChildElement(const QDomNode& node, const QString& tags)
{
    //tags is something like
    QStringList tagList = tags.split('/');
    QDomNode currentNode = node;
    for (const auto& tag : tagList)
    {
        currentNode = findChildElementByTag(currentNode, tag);
        if (currentNode.isNull())
            break;
    }
    QDomElement element = currentNode.toElement();
    return element;
}

/**
* For each item in the node 'node' reads its status.
* If at least one item has a status SMART_START sets StartStopStatus::start to true.
* If at least one item has a status SMART_STOP sets StartStopStatus::stop to true.
* It is quite common, that some objects have SMART_START status and some others have
* SMART_STOP status at the same time.
*/
EventStatus readEventStatus(const QDomNode& node)
{
    static const QString kItem = "item";
    static const QString kStatus = "status";
    static const QString kStartValue = "SMART_START";
    static const QString kStopValue = "SMART_STOP";

    EventStatus eventStatus;

    for (QDomNode item = node.firstChild(); !item.isNull(); item = item.nextSibling())
    {
        if (item.nodeName() == kItem)
        {
            const QString text = findChildElementByTag(item, kStatus).toElement().text();
            if (text == kStartValue)
                eventStatus |= EventState::startEvent;
            else if (text == kStopValue)
                eventStatus |= EventState::stopEvent;
        }
    }
    return eventStatus;
}

/**
* For each item in the node 'node' reads it ratio and theshold. If at least one ratio is bigger
* then the threshold, return true. Otherwise returns false.
*/
bool isRatioMoreThenTreshold(const QDomNode& node)
{
    static const QString kItem = "item";
    static const QString kThreshold = "alarmThreshold";
    static const QString kRatio = "ratio";

    for (QDomNode item = node.firstChild(); !item.isNull(); item = item.nextSibling())
    {
        if (item.nodeName() == kItem)
        {
            const QString threshold = findChildElementByTag(item, kThreshold).toElement().text();
            const QString ratio = findChildElementByTag(item, kRatio).toElement().text();

            // QString::toInt() return 0 if string doesn't contain int, this is ok.
            const int intThreshold = threshold.toInt();
            const int intRatio = ratio.toInt();

            if (intRatio > intThreshold)
                return true;
        }
    }
    return false;
}

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

    const QDomNode alarmStatusInfo = dom.firstChild().firstChild();
    QString c1 = alarmStatusInfo.nodeName();

    for (QDomNode alarm = alarmStatusInfo.firstChild(); !alarm.isNull(); alarm = alarm.nextSibling())
    {
        const QString alarmTag = alarm.nodeName();
        const QString alarmValue = alarm.toElement().text();
        const bool value = (alarmValue == "true");
        result.push_back(AlarmPair{ alarmTag.toUtf8(), value });
    }

    return result;
}

/**
* Extracts an event name form the notification and returns it. If the event should not be
* The camera is buggy, xls are sometimes not complete, its structure is inconsistent,
* so there are several wierd workarounds in the code.
*/
QByteArray getEventName(const QDomDocument& dom)
{
    // Here are tag names, used to obtain dom information.
    static const QString kSmartType = "smartType";
    static const QString kPerimeter = "perimeter";
    static const QString kTripwire = "tripwire";
    static const QString kPerimeterRule = "perimeterRule";
    static const QString kListInfo = "listInfo";

    static const QString kPerInfo = "perimeter/perInfo";
    static const QString kTripInfo = "tripwire/tripInfo";

    const QDomNode configNode = dom.firstChild();
    QByteArray alarmMessage =
        findChildElementByTag(configNode, kSmartType).text().toStdString().c_str();

    // Sometimes camera forgets to add smartType tag for PEA. So we should detect it by other tags.
    if (alarmMessage.isEmpty())
    {
        if (!findChildElementByTag(configNode, kPerimeter).isNull()
            && !findChildElementByTag(configNode, kTripwire).isNull())
        {
            alarmMessage = "PEA";
        }
    }

    // There is a strange feature. For PEA event (only) camera sends messages when event
    // options are changed or when event-notification is switched on/off.
    // So we should detect it and filter out.
    if (alarmMessage == "PEA")
    {
        if (!findChildElementByTag(configNode, kPerimeterRule).isNull())
            alarmMessage.clear();
    }

    // Only notifications with SMART_START status are taken into account.
    if (alarmMessage == "PEA")
    {
        // Status information is separate for each detected object, so we should read
        // status element for every item.

        const QDomElement perInfo =
            findNestedChildElement(configNode, kPerInfo);
        const EventStatus perStatus = readEventStatus(perInfo);

        const QDomElement tripInfo =
            findNestedChildElement(configNode, kTripInfo);
        const EventStatus tripStatus = readEventStatus(tripInfo);

        const EventStatus globalStatus(perStatus | tripStatus);

        if (!globalStatus.testFlag(EventState::startEvent))
            alarmMessage.clear();
    }

    // For CDD we send event to server only if current ratio is greater then the threshold.
    if (alarmMessage == "CDD")
    {
        // The value of ratio may be different for diferent items,
        // so we should read values for every item and choose maximum.
        const QDomElement listInfo = findChildElementByTag(configNode, kListInfo);
        if (!isRatioMoreThenTreshold(listInfo))
            alarmMessage.clear();
    }

    return alarmMessage;
}

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugin
} // namespace nx
