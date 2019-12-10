#include "system_event_log_reader.h"

#include "nx/utils/log/log.h"

#include <array>
#include <vector>
#include <algorithm>

#include <QString>
#include <QtXml/QDomElement>

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
        "Failed to read accepted event log notification message. LastError = %1.");
    if (auto error = GetLastError(); error != ERROR_INSUFFICIENT_BUFFER)
    {
        NX_DEBUG(systemEventLogReader, kErrorMessage.arg(error));
        return 0;
    }

    std::vector<TCHAR> buffer(bufferSize, TCHAR{});
    EvtRender(nullptr /*must be nullptr for EvtRenderEventXml*/, hEvent, EvtRenderEventXml,
        bufferSize, &buffer.front(),
        &bufferSize, &propertyCount);

    if (auto error = GetLastError(); GetLastError() != ERROR_SUCCESS)
    {
        NX_DEBUG(systemEventLogReader, kErrorMessage.arg(error));
        return 0;
    }

    NX_ASSERT(sizeof(TCHAR) == 2);
    QString xmlMessage = QString::fromUtf16(reinterpret_cast<ushort*>(&buffer.front()));
    xmlMessage.replace(QRegExp("\\s+"), " "); //< removing space sequences

    NX_DEBUG(systemEventLogReader, systemEventLogReader->makeDebugMessage(
        "System event notification read: %1.").arg(xmlMessage));

    const SystemEventNotificationInfo info = systemEventLogReader->parseXmlMessage(xmlMessage);
    NX_DEBUG(systemEventLogReader, systemEventLogReader->makeDebugMessage(
        "Notification parameters: provider = %1, level = %2, event id = %3, message = %4.")
        .arg(info.providerName, QString::number(info.level), QString::number(info.eventId), info.data));

    if (systemEventLogReader->isMessageSignificant(xmlMessage, info))
    {
        NX_DEBUG(systemEventLogReader, systemEventLogReader->makeDebugMessage(
            "System event notification is significant and is retransmitted to server: "
            "provider = %1, level = %2, event id = %3, message = %4.")
            .arg(info.providerName, QString::number(info.level), QString::number(info.eventId), info.data));

        emit systemEventLogReader->eventOccurred(info.data, systemEventLogReader->reason());
    }
    else
    {
        NX_DEBUG(systemEventLogReader, systemEventLogReader->makeDebugMessage(
            "System event notification is not significant and ignored: "
            "provider = %1, level = %2, event id = %3, message = %4.")
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
            NX_DEBUG(this, makeDebugMessage(
                "Subscription to system log event notifications succeeded."));
            return true;
        }

        const DWORD status = GetLastError();
        switch (status)
        {
            case ERROR_EVT_CHANNEL_NOT_FOUND:
                NX_DEBUG(this, makeDebugMessage(
                    "Subscribing to system log notifications failed. Channel %1 was not found.")
                    .arg(m_logName));
                break;
            case ERROR_EVT_INVALID_QUERY:
                NX_DEBUG(this, makeDebugMessage(
                    "Subscribing to system log notifications failed. The query \"%1\" is not valid.")
                    .arg(QString::fromUtf16(reinterpret_cast<const ushort *>(kXpathQuery))));
                break;
            case ERROR_ACCESS_DENIED:
                NX_DEBUG(this, makeDebugMessage(
                    "Subscribing to system log notifications failed. Access denied."));
                break;
            default:
                // unexpected error
                const int kBufferSize = 1024;
                WCHAR buffer[kBufferSize] = L"unknown"; //< default message if EvtGetExtendedStatus fail
                DWORD usedBufferSize = 0;
                EvtGetExtendedStatus(kBufferSize, buffer, &usedBufferSize);

                NX_DEBUG(this, makeDebugMessage(
                    "Subscribing to system log notifications failed. The reason is %1.")
                    .arg(QString::fromUtf16(reinterpret_cast<ushort *>(buffer))));
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

SystemEventNotificationInfo SystemEventLogReader::parseXmlMessage(const QString& xmlMessage)
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

    SystemEventNotificationInfo result;

    QDomDocument doc;
    doc.setContent(xmlMessage);
    QDomElement root = doc.documentElement();
    if (root.isNull() || root.tagName() != "Event")
        return result;

    QDomNode systemNode = root.firstChild();
    if (systemNode.isNull() || systemNode.toElement().tagName() != "System")
        return result;

    for (QDomNode child = systemNode.firstChild(); !child.isNull(); child = child.nextSibling())
    {
        if (child.toElement().tagName() == "Provider")
            result.providerName = child.attributes().namedItem("Name").toAttr().value();

        else if (child.toElement().tagName() == "EventID")
            result.eventId = child.toElement().text().toInt();

        else if (child.toElement().tagName() == "Level")
            result.level = child.toElement().text().toInt();
    }

    // EventData/Data information is not used for filtering, but contains useful information.
    QDomNode dataNode = systemNode.nextSibling();
    if (dataNode.isNull() || dataNode.toElement().tagName() != "EventData")
        return result;

    for (QDomNode child = dataNode.firstChild(); !child.isNull(); child = child.nextSibling())
    {
        if (child.toElement().tagName() == "Data")
        {
            result.data = child.toElement().text();
            break;
        }
    }
    return result;
}

bool SystemEventLogReader::isMessageSignificant(
    const QString& xmlMessage, const SystemEventNotificationInfo& parsedMessage)
{
    return true;
}

QString SystemEventLogReader::makeDebugMessage(const QString& text)
{
    return text + QString(" LogName = ") + m_logName;
}

RaidEventLogReader::RaidEventLogReader(QString logName, QString providerName)
    :
    SystemEventLogReader(std::move(logName), std::move(providerName),
        /*max notification level*/ 5, //< RaidEventLogReader ignores notification level.
        nx::vms::event::EventReason::raidStorageError)
{
}

bool RaidEventLogReader::isMessageSignificant(
    const QString& xmlMessage, const SystemEventNotificationInfo& parsedMessage)
{
    if (parsedMessage.providerName != providerName())
        return false;

    // These event identificators are selected by Daniel Gonzalez <d.gonzalez@hanwha.com>.
    const std::vector<int> significantIds = {
        0x003d, //<  61: Consistency Check failed on %s
        0x0044, //<  68: Initialization failed on %s
        0x0051, //<  81: State change on %s from %s to %s
        0x0065, //< 101: Rebuild failed on %s due to source drive error
        0x0066, //< 102: Rebuild failed on %s due to target drive error
        0x0070, //< 112: Drive removed: %s
        0x0096, //< 150: Battery has failed and cannot support data retention.Please replace the battery.
        0x00f8, //< 248: Removed PD: %s Info: %s
        0x00fa, //< 250: VD %s is now PARTIALLY DEGRADED
        0x00fb, //< 251: VD %s is now DEGRADED
        0x010c, //< 268: PD %s reset (Type %02x)
        0x011a, //< 282: Replace Drive failed on PD %s due to source %s error
        0x0191, //< 401: Diagnostics failed for %s
    };

    bool ok = std::find(significantIds.cbegin(), significantIds.cend(), parsedMessage.eventId)
        != significantIds.end();
    if (ok)
        return true;

    // Special case.
    if (parsedMessage.eventId == 114 //< State change.
        && parsedMessage.data.contains("Current = Failed", Qt::CaseInsensitive))
    {
        return true;
    }

    return false;
}
