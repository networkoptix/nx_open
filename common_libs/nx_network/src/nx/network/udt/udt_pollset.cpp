/**********************************************************
* Nov 25, 2015
* a.kolesnikov
***********************************************************/

#include "udt_pollset.h"

#include <udt/udt.h>

#include <nx/utils/log/log.h>

#include "udt_common.h"
#include "udt_socket.h"
#include "udt_socket_impl.h"


namespace nx {
namespace network {

// ========================================================
// PollSet implementation
// ========================================================
namespace detail {
namespace {
struct SocketUserData
{
    boost::optional<void*> read_data;
    boost::optional<void*> write_data;
    UdtSocket* socket;
    bool deleted;
    SocketUserData():socket(NULL), deleted(false) {}
};
}// namespace 

static const int kInterruptionBufferLength = 128;

class UdtPollSetConstIteratorImpl;

class UdtPollSetImpl
{
public:
    friend class UdtPollSetConstIteratorImpl;

    std::set<UDTSOCKET> udt_read_set_;
    std::set<UDTSOCKET> udt_write_set_;
    std::size_t m_size;
    std::map<UDTSOCKET, SocketUserData> socket_user_data_;
    int epoll_fd_;
    UDPSocket interrupt_socket_;
    std::set<SYSSOCKET> udt_sys_socket_read_set_;
    typedef std::map<UDTSOCKET, SocketUserData>::iterator socket_iterator;
    std::list<socket_iterator> reclaim_list_;
    std::set<UdtPollSetConstIteratorImpl*> iterators;

    UdtPollSetImpl():m_size(0), epoll_fd_(-1) {}
    bool initialize();

    bool initializeInterruptSocket();
    void removeFromEpoll(UDTSOCKET udt_handler, int event_type);
    int MapAioEventToUdtEvent(aio::EventType et)
    {
        switch (et)
        {
            case aio::etRead: return UDT_EPOLL_IN | UDT_EPOLL_ERR;
            case aio::etWrite:return UDT_EPOLL_OUT | UDT_EPOLL_ERR;
            default: NX_ASSERT(0); return 0;
        }
    }
    void reclaimSocket();
    // Right now UDT can give socket that is not supposed to be watched 
    void removePhantomSocket(std::set<UDTSOCKET>* set);

    static UdtPollSetImpl* Create();
};

void UdtPollSetImpl::removePhantomSocket(std::set<UDTSOCKET>* set)
{
    std::set<UDTSOCKET>::iterator ib = set->begin();
    while (ib != set->end())
    {
        std::map<UDTSOCKET, SocketUserData>::iterator ifind = socket_user_data_.find(*ib);
        if (ifind == socket_user_data_.end())
        {
            ib = set->erase(ib);
            //TODO #udt sometimes udt poll reports unknown sockets to us...
        }
        else
        {
            ++ib;
        }
    }
}

void UdtPollSetImpl::removeFromEpoll(UDTSOCKET udt_handler, int event_type)
{
    // UDT will remove all the related event type related to a certain UDT handler, so if a udt
    // handler is watching read/write, then after a remove, all the event type will be removed.
    int ret = UDT::epoll_remove_usock(epoll_fd_, udt_handler);
    if (event_type != UDT_EPOLL_ERR)
    {
        NX_ASSERT(event_type == UDT_EPOLL_IN || event_type == UDT_EPOLL_OUT);
        ret = UDT::epoll_add_usock(epoll_fd_, udt_handler, &event_type);
        if (ret != 0)
            SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    }
}

void UdtPollSetImpl::reclaimSocket()
{
    for (std::list<socket_iterator>::iterator ib = reclaim_list_.begin(); ib != reclaim_list_.end(); ++ib)
    {
        if ((*ib)->second.deleted)
            socket_user_data_.erase(*ib);
    }
    reclaim_list_.clear();
}

bool UdtPollSetImpl::initialize()
{
    NX_ASSERT(epoll_fd_ <0);
    epoll_fd_ = UDT::epoll_create();
    if (epoll_fd_ <0)
    {
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    return initializeInterruptSocket();
}

bool UdtPollSetImpl::initializeInterruptSocket()
{
    if (!interrupt_socket_.setNonBlockingMode(true) ||
        !interrupt_socket_.bind(SocketAddress(HostAddress::localhost, 0)))
    {
        return false;
    }

    // adding this iterrupt_socket_ to the epoll set
    int ret = UDT::epoll_add_ssock(epoll_fd_, interrupt_socket_.handle());
    if (ret <0)
    {
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    return true;
}

UdtPollSetImpl* UdtPollSetImpl::Create()
{
    std::unique_ptr<UdtPollSetImpl> ret(new UdtPollSetImpl());
    if (ret->initialize())
        return ret.release();
    else
        return NULL;
}

class UdtPollSetConstIteratorImpl
{
public:
    UdtPollSetConstIteratorImpl(UdtPollSetImpl* pollSetImpl, bool end)
    :
        m_pollSetImpl(pollSetImpl),
        m_inReadSet(!end)
    {
        if (end)
        {
            m_curPos = m_pollSetImpl->udt_write_set_.end();
        }
        else
        {
            m_curPos = m_pollSetImpl->udt_read_set_.begin();
            if (m_curPos == m_pollSetImpl->udt_read_set_.end())
            {
                m_curPos = m_pollSetImpl->udt_write_set_.begin();
                m_inReadSet = false;
            }
        }

        m_pollSetImpl->iterators.insert(this);
    }

    ~UdtPollSetConstIteratorImpl()
    {
        NX_ASSERT(m_pollSetImpl->iterators.erase(this) == 1);
    }

    void next()
    {
        NX_ASSERT(!done());
        if (m_inReadSet)
        {
            ++m_curPos;
            if (m_curPos == m_pollSetImpl->udt_read_set_.end())
            {
                m_inReadSet = false;
                m_curPos = m_pollSetImpl->udt_write_set_.begin();
            }
        }
        else
        {
            ++m_curPos;
        }
    }

    bool done() const
    {
        if (m_inReadSet)
        {
            return false;
        }
        else
        {
            return m_curPos == m_pollSetImpl->udt_write_set_.end();
        }
    }

    UdtSocket* getSocket() const
    {
        return GetSocketUserData()->socket;
    }

    void* getUserData() const
    {
        const SocketUserData* ref = GetSocketUserData();
        aio::EventType event_type = getEventType();
        if (event_type == aio::etRead)
        {
            return ref->read_data == boost::none ? NULL : ref->read_data.get();
        }
        else if (event_type == aio::etWrite)
        {
            return ref->write_data == boost::none ? NULL : ref->write_data.get();
        }
        else
        {
            // For an error, what we need to return , an NULL pointer ?
            return NULL;
        }
    }

    aio::EventType getEventType() const
    {
        NX_ASSERT(!done());
        if (m_inReadSet)
        {
            return aio::etRead;
        }
        else
        {
            // This work around will make the UDT::epoll_wait like 
            // posix epoll and not trapped into the UDT::epoll_wait 
            // bug with broken connection .
            int pending_epoll_event;
            int pending_epoll_event_size = sizeof(int);
            int ret = UDT::getsockopt(*m_curPos, 0, UDT_EVENT,
                &pending_epoll_event, &pending_epoll_event_size);
            if (ret != 0)
                return aio::etWrite;
            else
            {
                if (pending_epoll_event & UDT_EPOLL_ERR)
                    return aio::etError;
                else
                    return aio::etWrite;
            }
        }
    }

    bool equal(const UdtPollSetConstIteratorImpl& impl) const
    {
        // iterator from different set/map cannot compare 
        return m_inReadSet == impl.m_inReadSet &&
               m_curPos == impl.m_curPos;
    }

    bool inReadSet() const
    {
        return m_inReadSet;
    }

    std::set<UDTSOCKET>::iterator curPos() const
    {
        return m_curPos;
    }

private:
    const SocketUserData* GetSocketUserData() const
    {
        NX_ASSERT(!done());
        std::map<UDTSOCKET, SocketUserData>::
            iterator ib = m_pollSetImpl->socket_user_data_.find(*m_curPos);
        NX_ASSERT(ib != m_pollSetImpl->socket_user_data_.end());
        return &(ib->second);
    }

private:
    UdtPollSetImpl* m_pollSetImpl;
    bool m_inReadSet;
    std::set<UDTSOCKET>::iterator m_curPos;

    UdtPollSetConstIteratorImpl(const UdtPollSetConstIteratorImpl&);
    UdtPollSetConstIteratorImpl& operator=(const UdtPollSetConstIteratorImpl&);
};

}// namespace detail


 // =========================================
 // Const Iterator
 // =========================================

UdtPollSet::const_iterator::const_iterator(const_iterator&& right)
{
    m_impl = right.m_impl;
    right.m_impl = nullptr;
}

UdtPollSet::const_iterator::const_iterator(detail::UdtPollSetImpl* impl, bool end):
    m_impl(new detail::UdtPollSetConstIteratorImpl(impl, end))
{
}

UdtPollSet::const_iterator::~const_iterator()
{
    delete m_impl;
    m_impl = nullptr;
}

UdtPollSet::const_iterator& UdtPollSet::const_iterator::operator ++()
{
    NX_ASSERT(m_impl);
    m_impl->next();
    return *this;
}

bool UdtPollSet::const_iterator::operator==(const UdtPollSet::const_iterator& right) const
{
    if (this == &right)
        return true;
    if (m_impl)
    {
        if (right.m_impl)
            return m_impl->equal(*right.m_impl);
        else
            return false;
    }
    else
    {
        if (right.m_impl)
            return false;
        else
            return true;
    }
}

bool UdtPollSet::const_iterator::operator!=(const UdtPollSet::const_iterator& right) const
{
    return !(*this == right);
}

UdtSocket* UdtPollSet::const_iterator::socket()
{
    NX_ASSERT(m_impl);
    return m_impl->getSocket();
}

const UdtSocket* UdtPollSet::const_iterator::socket() const
{
    NX_ASSERT(m_impl);
    return m_impl->getSocket();
}

aio::EventType UdtPollSet::const_iterator::eventType() const
{
    NX_ASSERT(m_impl);
    return m_impl->getEventType();
}

void* UdtPollSet::const_iterator::userData()
{
    NX_ASSERT(m_impl);
    return m_impl->getUserData();
}

// ============================================
// UdtPollSet
// ============================================

UdtPollSet::UdtPollSet():
    m_impl(detail::UdtPollSetImpl::Create())
{
}

UdtPollSet::~UdtPollSet()
{
    delete m_impl;
    m_impl = nullptr;
}

void UdtPollSet::interrupt()
{
    char buffer[detail::kInterruptionBufferLength];
    /*bool ret = */m_impl->interrupt_socket_.sendTo(
        buffer,
        detail::kInterruptionBufferLength,
        m_impl->interrupt_socket_.getLocalAddress());
    //return ret;
}

bool UdtPollSet::add(UdtSocket* socket, aio::EventType eventType, void* userData)
{
    //NX_LOG(lit("UdtPollSet::add. sock %1, eventType %2").
    //    arg(static_cast<UDTSocketImpl*>(socket->impl())->udtHandle).
    //    arg((int)eventType), cl_logINFO);

    NX_ASSERT(socket != NULL);
    int ev = m_impl->MapAioEventToUdtEvent(eventType);
    int ret = UDT::epoll_add_usock(m_impl->epoll_fd_, static_cast<UDTSocketImpl*>(socket->impl())->udtHandle, &ev);
    if (ret <0)
    {
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    detail::SocketUserData& ref = m_impl->socket_user_data_[static_cast<UDTSocketImpl*>(socket->impl())->udtHandle];
    if (ev == (UDT_EPOLL_IN | UDT_EPOLL_ERR))
        ref.read_data = userData;
    else
        ref.write_data = userData;
    ref.socket = socket;
    ref.deleted = false;
    ++m_impl->m_size;
    return true;
}

void* UdtPollSet::remove(UdtSocket* socket, aio::EventType eventType)
{
    //NX_LOG(lit("UdtPollSet::remove. sock %1, eventType %2").
    //    arg(static_cast<UDTSocketImpl*>(socket->impl())->udtHandle).
    //    arg((int)eventType), cl_logINFO);

    NX_ASSERT(socket);
    if (eventType != aio::etRead && eventType != aio::etWrite)
    {
        NX_ASSERT(false);
        return nullptr;
    }

    const auto udtHandle = static_cast<UDTSocketImpl*>(socket->impl())->udtHandle;

    std::map<UDTSOCKET, detail::SocketUserData>::iterator
        ib = m_impl->socket_user_data_.find(udtHandle);
    if (ib == m_impl->socket_user_data_.end())
        return NULL;
    int remain_event_type = UDT_EPOLL_ERR;
    void* rudata = nullptr;

    switch (eventType)
    {
        case aio::etRead:
        {
            if (ib->second.read_data == boost::none)
                return NULL;
            rudata = ib->second.read_data.get();
            ib->second.read_data = boost::none;
            if (ib->second.write_data == boost::none)
            {
                ib->second.deleted = true;
                //not adding duplicate iterator
                if (std::find(m_impl->reclaim_list_.begin(), m_impl->reclaim_list_.end(), ib) == m_impl->reclaim_list_.end())
                    m_impl->reclaim_list_.push_back(ib);
            }
            else
            {
                // We have write operation for this objects
                remain_event_type = UDT_EPOLL_OUT;
            }
            auto udtHandleIter = m_impl->udt_read_set_.find(udtHandle);
            if (udtHandleIter == m_impl->udt_read_set_.end())
                break;
            bool canRemove = true;
            for (auto iter: m_impl->iterators)
            {
                if (iter->inReadSet() && (iter->curPos() == udtHandleIter))
                {
                    //cannot remove element since it is being used
                    canRemove = false;
                    break;
                }
            }
            if (canRemove)
                m_impl->udt_read_set_.erase(udtHandleIter);
            break;
        }

        case aio::etWrite:
        {
            if (ib->second.write_data == boost::none)
                return NULL;
            rudata = ib->second.write_data.get();
            ib->second.write_data = boost::none;
            if (ib->second.read_data == boost::none)
            {
                ib->second.deleted = true;
                if (std::find(m_impl->reclaim_list_.begin(), m_impl->reclaim_list_.end(), ib) == m_impl->reclaim_list_.end())
                    m_impl->reclaim_list_.push_back(ib);
            }
            else
            {
                remain_event_type = UDT_EPOLL_IN;
            }
            auto udtHandleIter = m_impl->udt_write_set_.find(udtHandle);
            if (udtHandleIter == m_impl->udt_write_set_.end())
                break;
            bool canRemove = true;
            for (auto iter : m_impl->iterators)
            {
                if (!iter->inReadSet() && (iter->curPos() == udtHandleIter))
                {
                    //cannot remove element since it is being used
                    canRemove = false;
                    break;
                }
            }
            if (canRemove)
                m_impl->udt_write_set_.erase(udtHandleIter);
            break;
        }

        default:
            NX_ASSERT(false);
            return nullptr;
    }

    m_impl->removeFromEpoll(udtHandle, remain_event_type);
    --m_impl->m_size;
    return rudata;
}

size_t UdtPollSet::size() const
{
    return m_impl->m_size;
}

void* UdtPollSet::getUserData(UdtSocket* socket, aio::EventType eventType) const
{
    NX_ASSERT(socket);
    std::map<UDTSOCKET, detail::SocketUserData>::const_iterator
        ib = m_impl->socket_user_data_.find(static_cast<UDTSocketImpl*>(socket->impl())->udtHandle);
    if (ib == m_impl->socket_user_data_.end())
        return NULL;
    else
    {
        switch (eventType)
        {
            case aio::etRead:
                return ib->second.read_data == boost::none ? NULL : ib->second.read_data.get();
            case aio::etWrite:
                return ib->second.write_data == boost::none ? NULL : ib->second.write_data.get();
            default: return NULL;
        }
    }
}

int UdtPollSet::poll(int millisToWait)
{
    m_impl->reclaimSocket();
    m_impl->udt_read_set_.clear();
    m_impl->udt_write_set_.clear();
    m_impl->udt_sys_socket_read_set_.clear();
    if (millisToWait == aio::INFINITE_TIMEOUT)
        millisToWait = -1;
    int ret = UDT::epoll_wait(
        m_impl->epoll_fd_, &m_impl->udt_read_set_, &m_impl->udt_write_set_, millisToWait, &m_impl->udt_sys_socket_read_set_, NULL);
    if (ret <0)
    {
        // UDT documentation is wrong, after "carefully" inspect its source code, the epoll_wait
        // for UDT will _NEVER_ return zero when time out, it will 1) return a positive number 
        // 2) -1 as time out or other error. The documentation says it will return zero . :(
        // And the ETIMEOUT's related error string is not even _IMPLEMENTED_ in its getErrorMessage
        // function !
        if (UDT::getlasterror_code() == CUDTException::ETIMEOUT)
        {
            return 0; // Translate this error code
        }
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return -1;
    }
    else
    {
        // remove control sockets
        if (!m_impl->udt_sys_socket_read_set_.empty())
        {
            char buffer[detail::kInterruptionBufferLength];
            m_impl->interrupt_socket_.recv(buffer, detail::kInterruptionBufferLength, 0);
            --ret;
        }
        // remove the phantom sockets
        m_impl->removePhantomSocket(&m_impl->udt_read_set_);
        m_impl->removePhantomSocket(&m_impl->udt_write_set_);
        return ret;
    }
}

UdtPollSet::const_iterator UdtPollSet::begin() const
{
    NX_ASSERT(m_impl);
    return const_iterator(m_impl, false);
}

UdtPollSet::const_iterator UdtPollSet::end() const
{
    NX_ASSERT(m_impl);
    return const_iterator(m_impl, true);
}

}   //network
}   //nx
