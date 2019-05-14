#include "system_event_log_reader.h"

#include "nx/utils/log/log.h"

#include <vector>

#include <QtCore/QString.h>

#ifdef WIN32
    #include <winevt.h>
    #pragma comment(lib, "wevtapi.lib")
#endif

namespace {
#ifdef WIN32

DWORD WINAPI SubscriptionCallback(
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

    if (systemEventLogReader->messageIsSignificant(xmlMessage))
    {
        emit systemEventLogReader->eventOccurred(QnResourcePtr(), systemEventLogReader->reason());
    }

    return 0; // this value is ignored
}
#endif

} // namespace

SystemEventLogReader::SystemEventLogReader(QString logName, nx::vms::event::EventReason reason):
    m_logName(std::move(logName)), m_reason(reason)
{
}

SystemEventLogReader::~SystemEventLogReader()
{
    unsubscribeRaidEvents();
}

bool SystemEventLogReader::subscribeRaidEvents()
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

void SystemEventLogReader::unsubscribeRaidEvents()
{
#ifdef WIN32
    if (m_hSubscription)
    {
        EvtClose(m_hSubscription);
        m_hSubscription = nullptr;
    }
#endif
}

QString SystemEventLogReader::makeDebugMessage(const QString& text)
{
    return text + QString(" LogName = ") + m_logName;
}
