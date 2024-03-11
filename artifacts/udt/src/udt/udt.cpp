#include "udt.h"

#if defined(_WIN32)
    #include <codecvt>
#endif

#include <cctype>
#include <cstring>

#include "core.h"

namespace UDT {

std::string toString(ProtocolError protocolError)
{
    switch (protocolError)
    {
        case ProtocolError::none:
            return "none";
        case ProtocolError::cannotListenInRendezvousMode:
            return "Cannot listen in rendezvous mode";
        case ProtocolError::handshakeFailure:
            return "Handshake failed";
        case ProtocolError::remotePeerInRendezvousMode:
            return "Remote peer is in rendezvous mode";
        case ProtocolError::retransmitReceived:
            return "retransmitReceived";
        case ProtocolError::outOfWindowDataReceived:
            return "outOfWindowDataReceived";
        default:
            return "unknown (" + std::to_string(static_cast<int>(protocolError)) + ")";
    }
}

//-------------------------------------------------------------------------------------------------

Error::Error(ProtocolError protocolError):
#ifdef _WIN32
    Error(GetLastError(), protocolError)
#else
    Error(errno, protocolError)
#endif
{
}

Error::Error(Errno osError, ProtocolError protocolError):
    m_osError(osError),
    m_protocolError(protocolError),
    m_errorText(prepareErrorText())
{
}

Errno Error::osError() const
{
    return m_osError;
}

ProtocolError Error::protocolError() const
{
    return m_protocolError;
}

const char* Error::errorText() const
{
    return m_errorText.c_str();
}

std::string Error::prepareErrorText()
{
    std::string text;

    // Adding "errno" information
    if (m_osError)
    {
#if defined(_WIN32)
        wchar_t msgBuf[1024];
        memset(msgBuf, 0, sizeof(msgBuf));

        const size_t msgLen = std::min<std::size_t>(
            FormatMessage(
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                m_osError,
                MAKELANGID(LANG_SYSTEM_DEFAULT, SUBLANG_SYS_DEFAULT),
                (LPTSTR)&msgBuf,
                sizeof(msgBuf),
                NULL),
            sizeof(msgBuf));

        if (msgLen > 0)
        {
            const auto msgUtf8Len = WideCharToMultiByte(CP_UTF8, 0, msgBuf, msgLen, NULL, 0, NULL, NULL);
            if (msgUtf8Len > 0)
            {
                text.resize(msgUtf8Len);
                if (WideCharToMultiByte(
                        CP_UTF8, 0, msgBuf, msgLen, text.data(), text.size(), NULL, NULL) <= 0)
                {
                    text = "Win32 API error " + std::to_string(m_osError);
                }
            }
            else
            {
                text = "Win32 API error " + std::to_string(m_osError);
            }

            while (!text.empty() && std::isspace(text.back()))
                text.pop_back();
        }
#else
        char errmsg[1024];
        if (strerror_r(m_osError, errmsg, 1024) == 0)
            text = errmsg;
#endif
    }

    if (m_protocolError != ProtocolError::none)
    {
        if (!text.empty())
            text += ". ";

        text += toString(m_protocolError);
    }

    return text;
}

//-------------------------------------------------------------------------------------------------

static int processResult(Result<> result)
{
    if (result.ok())
        return 0;

    CUDT::s_UDTUnited->setError(result.error());
    return -1;
}

template<
    typename Payload,
    typename = std::enable_if_t<std::is_signed_v<Payload>>
>
static Payload processResult(Result<Payload> result)
{
    if (result.ok())
        return result.get();

    CUDT::s_UDTUnited->setError(result.error());
    return -1;
}

//-------------------------------------------------------------------------------------------------

int startup()
{
    return processResult(CUDT::startup());
}

int cleanup()
{
    return processResult(CUDT::cleanup());
}

UDTSOCKET socket(int af, int type, int protocol)
{
    return processResult(CUDT::socket(af, type, protocol));
}

int bind(UDTSOCKET u, const struct sockaddr* name, int namelen)
{
    return processResult(CUDT::bind(u, name, namelen));
}

int bind2(UDTSOCKET u, UDPSOCKET udpsock)
{
    return processResult(CUDT::bind(u, udpsock));
}

int listen(UDTSOCKET u, int backlog)
{
    return processResult(CUDT::listen(u, backlog));
}

UDTSOCKET accept(UDTSOCKET u, struct sockaddr* addr, int* addrlen)
{
    return processResult(CUDT::accept(u, addr, addrlen));
}

int connect(UDTSOCKET u, const struct sockaddr* name, int namelen)
{
    return processResult(CUDT::connect(u, name, namelen));
}

int shutdown(UDTSOCKET u, int how)
{
    return processResult(CUDT::shutdown(u, how));
}

int close(UDTSOCKET u)
{
    return processResult(CUDT::close(u));
}

int getpeername(UDTSOCKET u, struct sockaddr* name, int* namelen)
{
    return processResult(CUDT::getpeername(u, name, namelen));
}

int getsockname(UDTSOCKET u, struct sockaddr* name, int* namelen)
{
    return processResult(CUDT::getsockname(u, name, namelen));
}

int getsockopt(UDTSOCKET u, int level, SOCKOPT optname, void* optval, int* optlen)
{
    return processResult(CUDT::getsockopt(u, level, optname, optval, optlen));
}

int setsockopt(UDTSOCKET u, int level, SOCKOPT optname, const void* optval, int optlen)
{
    return processResult(CUDT::setsockopt(u, level, optname, optval, optlen));
}

int send(UDTSOCKET u, const char* buf, int len, int flags)
{
    return processResult(CUDT::send(u, buf, len, flags));
}

int recv(UDTSOCKET u, char* buf, int len, int flags)
{
    return processResult(CUDT::recv(u, buf, len, flags));
}

int sendmsg(UDTSOCKET u, const char* buf, int len, int ttl, bool inorder)
{
    return processResult(CUDT::sendmsg(u, buf, len, ttl, inorder));
}

int recvmsg(UDTSOCKET u, char* buf, int len)
{
    return processResult(CUDT::recvmsg(u, buf, len));
}

int64_t sendfile(UDTSOCKET u, std::fstream& ifs, int64_t& offset, int64_t size, int block)
{
    return processResult(CUDT::sendfile(u, ifs, offset, size, block));
}

int64_t recvfile(UDTSOCKET u, std::fstream& ofs, int64_t& offset, int64_t size, int block)
{
    return processResult(CUDT::recvfile(u, ofs, offset, size, block));
}

int64_t sendfile2(UDTSOCKET u, const char* path, int64_t* offset, int64_t size, int block)
{
    std::fstream ifs(path, std::ios::binary | std::ios::in);
    int64_t ret = processResult(CUDT::sendfile(u, ifs, *offset, size, block));
    ifs.close();
    return ret;
}

int64_t recvfile2(UDTSOCKET u, const char* path, int64_t* offset, int64_t size, int block)
{
    std::fstream ofs(path, std::ios::binary | std::ios::out);
    int64_t ret = processResult(CUDT::recvfile(u, ofs, *offset, size, block));
    ofs.close();
    return ret;
}

int select(int nfds, UDSET* readfds, UDSET* writefds, UDSET* exceptfds, const struct timeval* timeout)
{
    return processResult(CUDT::select(nfds, readfds, writefds, exceptfds, timeout));
}

int selectEx(
    const std::vector<UDTSOCKET>& fds,
    std::vector<UDTSOCKET>* readfds,
    std::vector<UDTSOCKET>* writefds,
    std::vector<UDTSOCKET>* exceptfds,
    int64_t msTimeOut)
{
    return processResult(CUDT::selectEx(fds, readfds, writefds, exceptfds, msTimeOut));
}

int epoll_create()
{
    return processResult(CUDT::epoll_create());
}

int epoll_add_usock(int eid, UDTSOCKET u, const int* events)
{
    return processResult(CUDT::epoll_add_usock(eid, u, events));
}

int epoll_add_ssock(int eid, SYSSOCKET s, const int* events)
{
    return processResult(CUDT::epoll_add_ssock(eid, s, events));
}

int epoll_remove_usock(int eid, UDTSOCKET u)
{
    return processResult(CUDT::epoll_remove_usock(eid, u));
}

int epoll_remove_ssock(int eid, SYSSOCKET s)
{
    return processResult(CUDT::epoll_remove_ssock(eid, s));
}

int epoll_wait(
    int eid,
    std::map<UDTSOCKET, int>* readfds, std::map<UDTSOCKET, int>* writefds,
    int64_t msTimeOut,
    std::map<SYSSOCKET, int>* lrfds, std::map<SYSSOCKET, int>* lwfds)
{
    return processResult(CUDT::epoll_wait(eid, readfds, writefds, msTimeOut, lrfds, lwfds));
}

int epoll_interrupt_wait(int eid)
{
    return processResult(CUDT::epoll_interrupt_wait(eid));
}

#define SET_RESULT(val, num, fds, it) \
   if ((val != nullptr) && !val->empty()) \
   { \
      if (*num > static_cast<int>(val->size())) \
         *num = val->size(); \
      int count = 0; \
      for (it = val->begin(); it != val->end(); ++ it) \
      { \
         if (count >= *num) \
            break; \
         fds[count ++] = it->first; \
      } \
   }

int epoll_wait2(
    int eid,
    UDTSOCKET* readfds, int* rnum,
    UDTSOCKET* writefds, int* wnum,
    int64_t msTimeOut,
    SYSSOCKET* lrfds, int* lrnum,
    SYSSOCKET* lwfds, int* lwnum)
{
    // This API is an alternative format for epoll_wait, created for compatability with other languages.
    // Users need to pass in an array for holding the returned sockets, with the maximum array length
    // stored in *rnum, etc., which will be updated with returned number of sockets.

    std::map<UDTSOCKET, int> readset;
    std::map<UDTSOCKET, int> writeset;
    std::map<SYSSOCKET, int> lrset;
    std::map<SYSSOCKET, int> lwset;
    std::map<UDTSOCKET, int>* rval = nullptr;
    std::map<UDTSOCKET, int>* wval = nullptr;
    std::map<SYSSOCKET, int>* lrval = nullptr;
    std::map<SYSSOCKET, int>* lwval = nullptr;
    if ((readfds != nullptr) && (rnum != nullptr))
        rval = &readset;
    if ((writefds != nullptr) && (wnum != nullptr))
        wval = &writeset;
    if ((lrfds != nullptr) && (lrnum != nullptr))
        lrval = &lrset;
    if ((lwfds != nullptr) && (lwnum != nullptr))
        lwval = &lwset;

    auto result = CUDT::epoll_wait(eid, rval, wval, msTimeOut, lrval, lwval);
    if (result.ok() && result.get() > 0)
    {
        std::map<UDTSOCKET, int>::const_iterator i;
        SET_RESULT(rval, rnum, readfds, i);
        SET_RESULT(wval, wnum, writefds, i);
        std::map<SYSSOCKET, int>::const_iterator j;
        SET_RESULT(lrval, lrnum, lrfds, j);
        SET_RESULT(lwval, lwnum, lwfds, j);
    }

    return processResult(std::move(result));
}

int epoll_release(int eid)
{
    return processResult(CUDT::epoll_release(eid));
}

const Error& getlasterror()
{
    return CUDT::getlasterror();
}

int perfmon(UDTSOCKET u, TRACEINFO* perf, bool clear)
{
    return processResult(CUDT::perfmon(u, perf, clear));
}

UDTSTATUS getsockstate(UDTSOCKET u)
{
    return CUDT::getsockstate(u);
}

}  // namespace UDT
