#include "system_event_log_reader.h"

#include "nx/utils/log/log.h"

#include <array>
#include <vector>
#include <algorithm>

#include <QString>
#include <QtXml/QDomElement>

#ifdef WIN32
    #pragma comment(lib, "wevtapi.lib")
#endif

#ifdef WIN32

DWORD WINAPI SystemEventLogReader::SubscriptionCallback(
    EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context, EVT_HANDLE hEvent)
{
    if (action != EvtSubscribeActionDeliver)
        return 0;

    SystemEventLogReader* systemEventLogReader = static_cast<SystemEventLogReader*>(context);
    NX_DEBUG(systemEventLogReader,
        systemEventLogReader->makeDebugMessage("System event log notification accepted."));

    DWORD bufferSize = 0;
    DWORD propertyCount = 0;

    // Try to get the size of buffer needed to store the notification massage (in XML format).
    EvtRender(nullptr /*must be nullptr for EvtRenderEventXml*/, hEvent, EvtRenderEventXml,
        0 /*size of allocated buffer*/, nullptr /*buffer for rendered content*/,
        &bufferSize, &propertyCount);

    const QString kErrorMessage = systemEventLogReader->makeDebugMessage(
        "Failed to read accepted event log notification message.");
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        NX_DEBUG(systemEventLogReader, kErrorMessage);
        return 0;
    }

    std::vector<TCHAR> buffer(bufferSize, TCHAR{});
    EvtRender(nullptr /*must be nullptr for EvtRenderEventXml*/, hEvent, EvtRenderEventXml,
        bufferSize, &buffer.front(),
        &bufferSize, &propertyCount);

    if (GetLastError() != ERROR_SUCCESS)
    {
        NX_DEBUG(systemEventLogReader, kErrorMessage);
        return 0;
    }

    NX_ASSERT(sizeof(TCHAR) == 2);
    const QString xmlMessage = QString::fromUtf16(reinterpret_cast<ushort*>(&buffer.front()));
    NX_INFO(systemEventLogReader, systemEventLogReader->makeDebugMessage(
        "System event notification read: %1").arg(xmlMessage));

    const notificationInfo info = systemEventLogReader->parseXmlMessage(xmlMessage);
    NX_INFO(systemEventLogReader, systemEventLogReader->makeDebugMessage(
        "Notification parameters: provider = %1, level = %2, event id = %3, message = %4")
        .arg(info.providerName, QString::number(info.level), QString::number(info.eventId), info.data));

    if (systemEventLogReader->messageIsSignificant(xmlMessage, info))
    {
        NX_INFO(systemEventLogReader, systemEventLogReader->makeDebugMessage(
            "System event notification is significant and is retransmitted to server: "
            "provider = %1, level = %2, event id = %3, message = %4")
            .arg(info.providerName, QString::number(info.level), QString::number(info.eventId), info.data));

        emit systemEventLogReader->eventOccurred(info.data, systemEventLogReader->reason());
    }
    else
    {
        NX_INFO(systemEventLogReader, systemEventLogReader->makeDebugMessage(
            "System event notification is not significant and ignored: "
            "provider = %1, level = %2, event id = %3, message = %4")
            .arg(info.providerName, QString::number(info.level), QString::number(info.eventId), info.data));
    }

    return 0; // this value is ignored
}
#endif

SystemEventLogReader::SystemEventLogReader(QString logName, QString providerName, int maxLevel,
    nx::vms::event::EventReason reason)
    :
    m_logName(std::move(logName)), m_providerName(std::move(providerName)), m_maxLevel(maxLevel),
    m_reason(reason)
{
}

SystemEventLogReader::~SystemEventLogReader()
{
    unsubscribe();
}

bool SystemEventLogReader::subscribe()
{
    // Currently implemented for Windows only.
#ifdef WIN32
    if (m_hSubscription != nullptr)
    {
        NX_DEBUG(this, makeDebugMessage(
            "Server tried to subscribe to system log notifications repeatedly."));
        return false;
    }

    const LPCWSTR kXpathQuery = L"*";
    m_hSubscription = EvtSubscribe(
        nullptr /*session*/, nullptr /*signal*/, (const wchar_t*) m_logName.utf16(), kXpathQuery,
        nullptr /*bookmark*/, this /*context*/,
        (EVT_SUBSCRIBE_CALLBACK)SubscriptionCallback, EvtSubscribeToFutureEvents);
    if (m_hSubscription != nullptr)
    {
        NX_INFO(this, makeDebugMessage(
            "Subscription to system log event notifications succeeded."));
        return true;
    }

    const DWORD status = GetLastError();
    switch (status)
    {
    case ERROR_EVT_CHANNEL_NOT_FOUND:
        NX_DEBUG(this, makeDebugMessage(
            "Subscribing to system log notifications failed. Channel %1 was not found")
            .arg(m_logName));
        return false;
    case ERROR_EVT_INVALID_QUERY:
        NX_DEBUG(this, makeDebugMessage(
            "Subscribing to system log notifications failed. The query \"%1\" is not valid")
            .arg(QString::fromUtf16(reinterpret_cast<const ushort *>(kXpathQuery))));
        return false;
    case ERROR_ACCESS_DENIED:
        NX_DEBUG(this, makeDebugMessage(
            "Subscribing to system log notifications failed. Access denied."));
        return false;
    default:
        // unexpected error
        const int kBufferSize = 1024;
        WCHAR buffer[kBufferSize] = L"unknown"; //< default message if EvtGetExtendedStatus fail
        DWORD usedBufferSize = 0;
        EvtGetExtendedStatus(kBufferSize, buffer, &usedBufferSize);

        NX_DEBUG(this, makeDebugMessage(
            "Subscribing to system log notifications failed. The reason is %1")
            .arg(QString::fromUtf16(reinterpret_cast<ushort *>(buffer))));

        return false;
    }

#endif
    return false;
}

void SystemEventLogReader::unsubscribe()
{
#ifdef WIN32
    if (m_hSubscription)
    {
        EvtClose(m_hSubscription);
        m_hSubscription = nullptr;
    }
#endif
}

notificationInfo SystemEventLogReader::parseXmlMessage(const QString& xmlMessage)
{
    /*
    The example of the incoming message:
    <Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event">
        <System>
            <Provider Name="MR_MONITOR" />
            <EventID Qualifiers="0">250</EventID>
            <Level>3</Level>
            <Task>1</Task>
            <Keywords>0x80000000000000</Keywords>
            <TimeCreated SystemTime="2019-05-14T22:29:54.592171200Z" />
            <EventRecordID>2953</EventRecordID>
            <Channel>Application</Channel>
            <Computer>DESKTOP-VFFMHR7</Computer>
            <Security />
        </System>
        <EventData>
            <Data>Controller ID: 0 VD is now PARTIALLY DEGRADED VD 0 Event ID:250</Data>
        </EventData>
    </Event>

    Important fields:
    Event/System/Provider
    Event/System/Level
    Event/System/EventID
    Event/EventData/Data

    */

    notificationInfo result;

    QDomDocument doc;
    doc.setContent(xmlMessage);
    QDomElement root = doc.documentElement();
    if (root.isNull() || root.tagName() != "Event")
        return result;

    QDomNode systemNode = root.firstChild();
    if (systemNode.isNull() || systemNode.toElement().tagName() != "System")
        return result;

    QDomNode systemChild = systemNode.firstChild();
    while (!systemChild.isNull())
    {
        if (systemChild.toElement().tagName() == "Provider")
            result.providerName = systemChild.attributes().namedItem("Name").toAttr().value();

        else if (systemChild.toElement().tagName() == "EventID")
            result.eventId = systemChild.toElement().text().toInt();

        else if (systemChild.toElement().tagName() == "Level")
            result.level = systemChild.toElement().text().toInt();

        systemChild = systemChild.nextSibling();
    }

    // EventData/Data information is not used for filtering, but contains useful information.
    QDomNode dataNode = systemNode.nextSibling();
    if (dataNode.isNull() || dataNode.toElement().tagName() != "EventData")
        return result;

    QDomNode dataChild = dataNode.firstChild();
    while (!dataChild.isNull())
    {
        if (dataChild.toElement().tagName() == "Data")
        {
            result.data = dataChild.toElement().text();
            break;
        }
        dataChild = dataChild.nextSibling();
    }
    return result;
}

bool SystemEventLogReader::messageIsSignificant(
    const QString& xmlMessage, const notificationInfo& parsedMessage)
{
    return true;
}

QString SystemEventLogReader::makeDebugMessage(const QString& text)
{
    return text + QString(" LogName = ") + m_logName;
}

RaidEventLogReader::RaidEventLogReader(QString logName, QString providerName, int maxLevel)
    :
    SystemEventLogReader(std::move(logName), std::move(providerName), maxLevel,
        nx::vms::event::EventReason::raidStorageError)
{
}

bool RaidEventLogReader::messageIsSignificant(
    const QString& xmlMessage, const notificationInfo& parsedMessage)
{
    if (parsedMessage.providerName != providerName())
        return false;
    // message level is ignored

    // These event identificators are selected by Daniel Gonzalez <d.gonzalez@hanwha.com>
    const std::array<int, 11> significantIds = {
        0x0044, //<  68
        0x0070, //< 112
        0x011a, //< 282
        0x0191, //< 401
        0x003d, //<  61
        0x0065, //< 101
        0x0066, //< 102
        0x0096, //< 150
        0x00fa, //< 250
        0x00fb, //< 251
        0x00f8 //< 248
    };

    bool ok = std::find(significantIds.cbegin(), significantIds.cend(), parsedMessage.eventId)
        != significantIds.end();
    if (ok)
        return true;

    // special case
    if (parsedMessage.eventId == 114 //< State change
        && parsedMessage.data.contains("Current = Failed"))
    {
        return true;
    }

    return false;
}
