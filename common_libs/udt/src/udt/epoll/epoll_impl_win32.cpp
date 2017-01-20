#include "epoll_impl_win32.h"

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

CEPollDescWin32::CEPollDescWin32():
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

CEPollDescWin32::~CEPollDescWin32()
{
    freeFDSet(m_readfds);
    m_readfds = NULL;
    freeFDSet(m_writefds);
    m_writefds = NULL;
    freeFDSet(m_exceptfds);
    m_exceptfds = NULL;

    freeInterruptSocket();
}

void CEPollDescWin32::add(const SYSSOCKET& s, const int* events)
{
    int& eventMask = m_sLocals[s];
    eventMask |= *events;
}

void CEPollDescWin32::remove(const SYSSOCKET& s)
{
    m_sLocals.erase(s);
}

std::size_t CEPollDescWin32::socketsPolledCount() const
{
    return m_sLocals.size();
}

int CEPollDescWin32::poll(
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds,
    std::chrono::microseconds timeout)
{
    using namespace std::chrono;

    prepareForPolling(lrfds, lwfds);

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
    prepareOutEvents(lrfds, lwfds, &receivedInterruptEvent);
    if (receivedInterruptEvent)
        --eventCount;

    return eventCount;
}

void CEPollDescWin32::interrupt()
{
    char buf[128];
    sendto(
        m_interruptionSocket,
        buf, sizeof(buf),
        0,
        (const sockaddr*)&m_interruptionSocketLocalAddress,
        sizeof(m_interruptionSocketLocalAddress));
}

void CEPollDescWin32::prepareForPolling(
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds)
{
    m_readfds->fd_count = 0;
    m_writefds->fd_count = 0;
    m_exceptfds->fd_count = 0;

    reallocFdSetIfNeeded(m_sLocals.size()+1, &m_readfdsCapacity, &m_readfds); //< +1 is for m_interruptionSocket
    reallocFdSetIfNeeded(m_sLocals.size(), &m_writefdsCapacity, &m_writefds);
    reallocFdSetIfNeeded(m_sLocals.size(), &m_exceptfdsCapacity, &m_exceptfds);

    m_readfds->fd_array[m_readfds->fd_count++] = m_interruptionSocket;

    for (const std::pair<SYSSOCKET, int>& fdAndEventMask : m_sLocals)
    {
        if (lrfds && (fdAndEventMask.second & UDT_EPOLL_IN) > 0)
        {
            m_readfds->fd_array[m_readfds->fd_count++] = fdAndEventMask.first;
        }
        if (lwfds && (fdAndEventMask.second & UDT_EPOLL_OUT) > 0)
        {
            m_writefds->fd_array[m_writefds->fd_count++] = fdAndEventMask.first;
        }
        m_exceptfds->fd_array[m_exceptfds->fd_count++] = fdAndEventMask.first;
    }
}

void CEPollDescWin32::prepareOutEvents(
    std::map<SYSSOCKET, int>* lrfds,
    std::map<SYSSOCKET, int>* lwfds,
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
        (*lrfds)[m_readfds->fd_array[i]] = UDT_EPOLL_IN;
    }

    for (size_t i = 0; i < m_writefds->fd_count; ++i)
        (*lwfds)[m_writefds->fd_array[i]] = UDT_EPOLL_OUT;

    for (size_t i = 0; i < m_exceptfds->fd_count; ++i)
    {
        if (lrfds)
            (*lrfds)[m_exceptfds->fd_array[i]] |= UDT_EPOLL_ERR;
        if (lwfds)
            (*lwfds)[m_exceptfds->fd_array[i]] |= UDT_EPOLL_ERR;
    }
}

void CEPollDescWin32::initializeInterruptSocket()
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

void CEPollDescWin32::freeInterruptSocket()
{
    if (m_interruptionSocket == INVALID_SOCKET)
        return;
    ::closesocket(m_interruptionSocket);
    m_interruptionSocket = INVALID_SOCKET;
}

#endif // _WIN32
