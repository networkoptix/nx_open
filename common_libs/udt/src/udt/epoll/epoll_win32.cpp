#include "epoll_win32.h"

#ifdef _WIN32

namespace {

static CustomFdSet* allocFDSet(size_t setSize)
{
    //NOTE: SOCKET is a pointer
    return (CustomFdSet*)malloc(sizeof(CustomFdSet) +
        sizeof(SOCKET)*(setSize - kInitialFdSetSize));
}

static void freeFDSet(CustomFdSet* fdSet)
{
    ::free(fdSet);
}

/**
 * Reallocs fdSet to be of size newSize while preseving existing data.
 * In case of allocation error *fdSet is freed and set to NULL.
 */
static void reallocFDSet(CustomFdSet** fdSet, size_t newSize)
{
    CustomFdSet* newSet = allocFDSet(newSize);

    //copying existing data
    if (*fdSet && newSet)
    {
        newSet->fd_count = (*fdSet)->fd_count;
        memcpy(newSet->fd_array, (*fdSet)->fd_array, (*fdSet)->fd_count * sizeof(*((*fdSet)->fd_array)));
    }

    freeFDSet(*fdSet);
    *fdSet = newSet;
}

static void reallocFdSetIfNeeded(
    size_t desiredSize,
    size_t* currentCapacity,
    CustomFdSet** fdSet)
{
    if (*currentCapacity < desiredSize)
    {
        *currentCapacity = desiredSize + kFdSetIncreaseStep;
        reallocFDSet(fdSet, *currentCapacity);
    }
}

} // namespace

EpollWin32::EpollWin32():
    m_interruptionSocket(INVALID_SOCKET),
    m_readfds(allocFDSet(kInitialFdSetSize)),
    m_readfdsCapacity(kInitialFdSetSize),
    m_writefds(allocFDSet(kInitialFdSetSize)),
    m_writefdsCapacity(kInitialFdSetSize),
    m_exceptfds(allocFDSet(kInitialFdSetSize)),
    m_exceptfdsCapacity(kInitialFdSetSize)
{
    memset(&m_interruptionSocketLocalAddress, 0, sizeof(m_interruptionSocketLocalAddress));
    initializeInterruptSocket();
}

EpollWin32::~EpollWin32()
{
    freeFDSet(m_readfds);
    m_readfds = NULL;
    freeFDSet(m_writefds);
    m_writefds = NULL;
    freeFDSet(m_exceptfds);
    m_exceptfds = NULL;

    freeInterruptSocket();
}

void EpollWin32::add(const SYSSOCKET& s, const int* events)
{
    int& eventMask = m_socketDescriptorToEventMask[s];
    eventMask |= *events;
}

void EpollWin32::remove(const SYSSOCKET& s)
{
    m_socketDescriptorToEventMask.erase(s);
}

std::size_t EpollWin32::socketsPolledCount() const
{
    return m_socketDescriptorToEventMask.size();
}

int EpollWin32::poll(
    std::map<SYSSOCKET, int>* socketsAvailableForReading,
    std::map<SYSSOCKET, int>* socketsAvailableForWriting,
    std::chrono::microseconds timeout)
{
    using namespace std::chrono;

    prepareForPolling(socketsAvailableForReading, socketsAvailableForWriting);

    const bool isTimeoutSpecified = timeout != microseconds::max();
    timeval tv;
    memset(&tv, 0, sizeof(tv));
    if (isTimeoutSpecified)
    {
        const auto fullSeconds = duration_cast<seconds>(timeout);
        tv.tv_sec = fullSeconds.count();
        tv.tv_usec = duration_cast<microseconds>(timeout - fullSeconds).count();
    }

    int eventCount = ::select(
        0,
        m_readfds->fd_count > 0
            ? reinterpret_cast<fd_set*>(m_readfds)
            : NULL,
        m_writefds->fd_count > 0
            ? reinterpret_cast<fd_set*>(m_writefds)
            : NULL,
        m_exceptfds->fd_count > 0
            ? reinterpret_cast<fd_set*>(m_exceptfds)
            : NULL,
        isTimeoutSpecified ? &tv : NULL);
    if (eventCount < 0)
        return -1;

    //select sets fd_count to number of sockets triggered and 
    //moves those descriptors to the beginning of fd_array
    if (eventCount == 0)
        return eventCount;

    bool receivedInterruptEvent = false;
    prepareOutEvents(socketsAvailableForReading, socketsAvailableForWriting, &receivedInterruptEvent);
    if (receivedInterruptEvent)
        --eventCount;

    return eventCount;
}

void EpollWin32::interrupt()
{
    char buf[128];
    sendto(
        m_interruptionSocket,
        buf, sizeof(buf),
        0,
        (const sockaddr*)&m_interruptionSocketLocalAddress,
        sizeof(m_interruptionSocketLocalAddress));
}

void EpollWin32::prepareForPolling(
    std::map<SYSSOCKET, int>* socketsAvailableForReading,
    std::map<SYSSOCKET, int>* socketsAvailableForWriting)
{
    m_readfds->fd_count = 0;
    m_writefds->fd_count = 0;
    m_exceptfds->fd_count = 0;

    reallocFdSetIfNeeded(m_socketDescriptorToEventMask.size()+1, &m_readfdsCapacity, &m_readfds); //< +1 is for m_interruptionSocket
    reallocFdSetIfNeeded(m_socketDescriptorToEventMask.size(), &m_writefdsCapacity, &m_writefds);
    reallocFdSetIfNeeded(m_socketDescriptorToEventMask.size(), &m_exceptfdsCapacity, &m_exceptfds);

    m_readfds->fd_array[m_readfds->fd_count++] = m_interruptionSocket;

    for (const std::pair<SYSSOCKET, int>& fdAndEventMask: m_socketDescriptorToEventMask)
    {
        if (socketsAvailableForReading && (fdAndEventMask.second & UDT_EPOLL_IN) > 0)
        {
            m_readfds->fd_array[m_readfds->fd_count++] = fdAndEventMask.first;
        }
        if (socketsAvailableForWriting && (fdAndEventMask.second & UDT_EPOLL_OUT) > 0)
        {
            m_writefds->fd_array[m_writefds->fd_count++] = fdAndEventMask.first;
        }
        m_exceptfds->fd_array[m_exceptfds->fd_count++] = fdAndEventMask.first;
    }
}

void EpollWin32::prepareOutEvents(
    std::map<SYSSOCKET, int>* socketsAvailableForReading,
    std::map<SYSSOCKET, int>* socketsAvailableForWriting,
    bool* receivedInterruptEvent)
{
    // Using win32-specific select features to get O(1) here.
    for (size_t i = 0; i < m_readfds->fd_count; ++i)
    {
        if (m_readfds->fd_array[i] == m_interruptionSocket)
        {
            char buf[128];
            recv(m_interruptionSocket, buf, sizeof(buf), 0);
            *receivedInterruptEvent = true;
            continue;
        }
        (*socketsAvailableForReading)[m_readfds->fd_array[i]] = UDT_EPOLL_IN;

        // TODO: #ak Rollback after fixing issue VMS-6814.
        if ((m_socketDescriptorToEventMask[m_readfds->fd_array[i]] & UDT_EPOLL_IN) == 0)
        {
            int* x = nullptr;
            *x = 1234567;
        }
    }

    for (size_t i = 0; i < m_writefds->fd_count; ++i)
        (*socketsAvailableForWriting)[m_writefds->fd_array[i]] = UDT_EPOLL_OUT;

    for (size_t i = 0; i < m_exceptfds->fd_count; ++i)
    {
        if (socketsAvailableForReading &&
            isPollingSocketForEvent(m_exceptfds->fd_array[i], UDT_EPOLL_IN))
        {
            (*socketsAvailableForReading)[m_exceptfds->fd_array[i]] |= UDT_EPOLL_ERR;
        }
        if (socketsAvailableForWriting &&
            isPollingSocketForEvent(m_exceptfds->fd_array[i], UDT_EPOLL_OUT))
        {
            (*socketsAvailableForWriting)[m_exceptfds->fd_array[i]] |= UDT_EPOLL_ERR;
        }
    }
}

bool EpollWin32::isPollingSocketForEvent(SYSSOCKET handle, int eventMask) const
{
    auto it = m_socketDescriptorToEventMask.find(handle);
    return it != m_socketDescriptorToEventMask.end()
        ? (it->second & eventMask) > 0
        : false;
}

void EpollWin32::initializeInterruptSocket()
{
    m_interruptionSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_interruptionSocket < 0)
        throw CUDTException(-1, 0, GetLastError());

    u_long val = 1;
    if (ioctlsocket(m_interruptionSocket, FIONBIO, &val) != 0)
    {
        ::closesocket(m_interruptionSocket);
        m_interruptionSocket = INVALID_SOCKET;
        throw CUDTException(-1, 0, GetLastError());
    }

    sockaddr_in localEndpoint;
    memset(&localEndpoint, 0, sizeof(localEndpoint));
    localEndpoint.sin_family = AF_INET;
    localEndpoint.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (::bind(
            m_interruptionSocket,
            (SOCKADDR*)&localEndpoint,
            sizeof(localEndpoint)) == SOCKET_ERROR)
    {
        ::closesocket(m_interruptionSocket);
        m_interruptionSocket = INVALID_SOCKET;
        throw CUDTException(-1, 0, GetLastError());
    }

    int addrLen = sizeof(m_interruptionSocketLocalAddress);
    if (::getsockname(
            m_interruptionSocket,
            (sockaddr*)&m_interruptionSocketLocalAddress,
            &addrLen) < 0)
    {
        ::closesocket(m_interruptionSocket);
        m_interruptionSocket = INVALID_SOCKET;
        throw CUDTException(-1, 0, GetLastError());
    }
}

void EpollWin32::freeInterruptSocket()
{
    if (m_interruptionSocket == INVALID_SOCKET)
        return;
    ::closesocket(m_interruptionSocket);
    m_interruptionSocket = INVALID_SOCKET;
}

#endif // _WIN32
