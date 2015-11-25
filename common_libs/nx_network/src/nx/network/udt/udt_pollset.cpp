/**********************************************************
* Nov 25, 2015
* a.kolesnikov
***********************************************************/

#include "udt_pollset.h"

#include <udt/udt.h>

#include "udt_common.h"
#include "udt_socket.h"
#include "udt_socket_impl.h"

#ifdef __APPLE__
static const int EPOLL_SIZE = 1024;
#else
//TODO #ak rebuild udt for linux and win32
#define EPOLL_SIZE
#endif


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

UdtPollSetImplPtr::UdtPollSetImplPtr(UdtPollSetImpl* imp):
    std::unique_ptr<UdtPollSetImpl>(imp) {}

UdtPollSetImplPtr::~UdtPollSetImplPtr() {}

UdtPollSetConstIteratorImplPtr::UdtPollSetConstIteratorImplPtr(UdtPollSetConstIteratorImpl* imp) :
    std::unique_ptr<UdtPollSetConstIteratorImpl>(imp) {}

UdtPollSetConstIteratorImplPtr::UdtPollSetConstIteratorImplPtr() {}

UdtPollSetConstIteratorImplPtr::~UdtPollSetConstIteratorImplPtr() {}

static const int kInterruptionBufferLength = 128;

class UdtPollSetImpl
{
public:
    friend class UdtPollSetConstIteratorImpl;

    std::set<UDTSOCKET> udt_read_set_;
    std::set<UDTSOCKET> udt_write_set_;
    std::size_t size_;
    std::map<UDTSOCKET, SocketUserData> socket_user_data_;
    int epoll_fd_;
    UDPSocket interrupt_socket_;
    std::set<SYSSOCKET> udt_sys_socket_read_set_;
    typedef std::map<UDTSOCKET, SocketUserData>::iterator socket_iterator;
    std::list<socket_iterator> reclaim_list_;

    UdtPollSetImpl():size_(0), epoll_fd_(-1) {}
    bool Initialize();

    bool InitializeInterruptSocket();
    void RemoveFromEpoll(UDTSOCKET udt_handler, int event_type);
    int MapAioEventToUdtEvent(aio::EventType et)
    {
        switch (et)
        {
            case aio::etRead: return UDT_EPOLL_IN | UDT_EPOLL_ERR;
            case aio::etWrite:return UDT_EPOLL_OUT | UDT_EPOLL_ERR;
            default: Q_ASSERT(0); return 0;
        }
    }
    void ReclaimSocket();
    // Right now UDT can give socket that is not supposed to be watched 
    void RemovePhantomSocket(std::set<UDTSOCKET>* set);

    static UdtPollSetImpl* Create();
};

void UdtPollSetImpl::RemovePhantomSocket(std::set<UDTSOCKET>* set)
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

void UdtPollSetImpl::RemoveFromEpoll(UDTSOCKET udt_handler, int event_type)
{
    // UDT will remove all the related event type related to a certain UDT handler, so if a udt
    // handler is watching read/write, then after a remove, all the event type will be removed.
    int ret = UDT::epoll_remove_usock(epoll_fd_, udt_handler);
    if (event_type != UDT_EPOLL_ERR)
    {
        Q_ASSERT(event_type == UDT_EPOLL_IN || event_type == UDT_EPOLL_OUT);
        ret = UDT::epoll_add_usock(epoll_fd_, udt_handler, &event_type);
        if (ret != 0)
            SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
    }
}

void UdtPollSetImpl::ReclaimSocket()
{
    for (std::list<socket_iterator>::iterator ib = reclaim_list_.begin(); ib != reclaim_list_.end(); ++ib)
    {
        if ((*ib)->second.deleted)
            socket_user_data_.erase(*ib);
    }
    reclaim_list_.clear();
}

bool UdtPollSetImpl::Initialize()
{
    Q_ASSERT(epoll_fd_ <0);
    epoll_fd_ = UDT::epoll_create(EPOLL_SIZE);
    if (epoll_fd_ <0)
    {
        SystemError::setLastErrorCode(detail::convertToSystemError(UDT::getlasterror().getErrorCode()));
        return false;
    }
    return InitializeInterruptSocket();
}

bool UdtPollSetImpl::InitializeInterruptSocket()
{
    interrupt_socket_.setNonBlockingMode(true);
    interrupt_socket_.bind(SocketAddress(HostAddress::localhost, 0));
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
    if (ret->Initialize())
        return ret.release();
    else
        return NULL;
}

class UdtPollSetConstIteratorImpl
{
public:

    UdtPollSetConstIteratorImpl(UdtPollSetImpl* impl, bool end):
        m_impl(impl),
        in_read_set_(!end)
    {
        if (end)
        {
            iterator_ = impl->udt_write_set_.end();
        }
        else
        {
            iterator_ = impl->udt_read_set_.begin();
            if (iterator_ == m_impl->udt_read_set_.end())
            {
                iterator_ = m_impl->udt_write_set_.begin();
                in_read_set_ = false;
            }
        }
    }

    UdtPollSetConstIteratorImpl(const UdtPollSetConstIteratorImpl& impl):
        m_impl(impl.m_impl),
        in_read_set_(impl.in_read_set_),
        iterator_(impl.iterator_)
    {
    }

    UdtPollSetConstIteratorImpl& operator = (const UdtPollSetConstIteratorImpl& impl)
    {
        if (this == &impl) return *this;
        m_impl = impl.m_impl;
        in_read_set_ = impl.in_read_set_;
        iterator_ = impl.iterator_;
        return *this;
    }

    void Next()
    {
        Q_ASSERT(!Done());
        if (in_read_set_)
        {
            ++iterator_;
            if (iterator_ == m_impl->udt_read_set_.end())
            {
                in_read_set_ = false;
                iterator_ = m_impl->udt_write_set_.begin();
            }
        }
        else
        {
            ++iterator_;
        }
    }

    bool Done() const
    {
        if (in_read_set_)
            return false;
        else
        {
            return iterator_ == m_impl->udt_write_set_.end();
        }
    }

    UdtSocket* GetSocket() const
    {
        return GetSocketUserData()->socket;
    }

    void* getUserData() const
    {
        const SocketUserData* ref = GetSocketUserData();
        aio::EventType event_type = GetEventType();
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

    aio::EventType GetEventType() const
    {
        Q_ASSERT(!Done());
        if (in_read_set_)
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
            int ret = UDT::getsockopt(*iterator_, 0, UDT_EVENT,
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

    bool Equal(const UdtPollSetConstIteratorImpl& impl) const
    {
        // iterator from different set/map cannot compare 
        bool result = in_read_set_ ^ impl.in_read_set_;
        if (result)
        {
            return false;
        }
        else
        {
            return iterator_ == impl.iterator_;
        }
    }
private:

    const SocketUserData* GetSocketUserData() const
    {
        Q_ASSERT(!Done());
        std::map<UDTSOCKET, SocketUserData>::
            iterator ib = m_impl->socket_user_data_.find(*iterator_);
        Q_ASSERT(ib != m_impl->socket_user_data_.end());
        return &(ib->second);
    }

private:
    UdtPollSetImpl* m_impl;
    bool in_read_set_;
    std::set<UDTSOCKET>::iterator iterator_;
};

}// namespace detail


 // =========================================
 // Const Iterator
 // =========================================

UdtPollSet::const_iterator::const_iterator() {}

UdtPollSet::const_iterator::const_iterator(const const_iterator& it)
{
    if (it.m_impl)
        m_impl.reset(new detail::UdtPollSetConstIteratorImpl(*it.m_impl));
}

UdtPollSet::const_iterator::const_iterator(detail::UdtPollSetImpl* impl, bool end):
    m_impl(new detail::UdtPollSetConstIteratorImpl(impl, end))
{
}

UdtPollSet::const_iterator& UdtPollSet::const_iterator::operator =(const UdtPollSet::const_iterator& ib)
{
    if (this == &ib) return *this;
    if (!m_impl)
    {
        if (ib.m_impl)
            m_impl.reset(new detail::UdtPollSetConstIteratorImpl(*ib.m_impl));
        else
            m_impl.reset();
    }
    else
    {
        if (ib.m_impl)
        {
            *m_impl = *ib.m_impl;
        }
        else
        {
            m_impl.reset();
        }
    }
    return *this;
}

UdtPollSet::const_iterator& UdtPollSet::const_iterator::operator ++()
{
    Q_ASSERT(m_impl);
    m_impl->Next();
    return *this;
}

UdtPollSet::const_iterator UdtPollSet::const_iterator::operator++(int)
{
    UdtPollSet::const_iterator self(*this);
    m_impl->Next();
    return self;
}

bool UdtPollSet::const_iterator::operator==(const UdtPollSet::const_iterator& right) const
{
    if (this == &right) return true;
    if (m_impl)
    {
        if (right.m_impl)
            return m_impl->Equal(*right.m_impl);
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
    Q_ASSERT(m_impl);
    return m_impl->GetSocket();
}

const UdtSocket* UdtPollSet::const_iterator::socket() const
{
    Q_ASSERT(m_impl);
    return m_impl->GetSocket();
}

aio::EventType UdtPollSet::const_iterator::eventType() const
{
    Q_ASSERT(m_impl);
    return m_impl->GetEventType();
}

void* UdtPollSet::const_iterator::userData()
{
    Q_ASSERT(m_impl);
    return m_impl->getUserData();
}

UdtPollSet::const_iterator::~const_iterator() {}

// ============================================
// UdtPollSet
// ============================================

UdtPollSet::UdtPollSet():
    m_impl(detail::UdtPollSetImpl::Create())
{
}

UdtPollSet::~UdtPollSet() {}

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
    Q_ASSERT(socket != NULL);
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
    ++m_impl->size_;
    return true;
}

void* UdtPollSet::remove(UdtSocket* socket, aio::EventType eventType)
{
    assert(socket);
    switch (eventType)
    {
        case aio::etRead:
        {
            std::map<UDTSOCKET, detail::SocketUserData>::iterator
                ib = m_impl->socket_user_data_.find(static_cast<UDTSocketImpl*>(socket->impl())->udtHandle);
            if (ib == m_impl->socket_user_data_.end() || ib->second.read_data == boost::none)
                return NULL;
            void* rudata = ib->second.read_data.get();
            int remain_event_type = UDT_EPOLL_ERR;
            ib->second.read_data = boost::none;
            if (ib->second.write_data == boost::none)
            {
                ib->second.deleted = true;
                m_impl->reclaim_list_.push_back(ib);
            }
            else
            {
                // We have write operation for this objects
                remain_event_type = UDT_EPOLL_OUT;
            }
            m_impl->RemoveFromEpoll(static_cast<UDTSocketImpl*>(socket->impl())->udtHandle, remain_event_type);
            --m_impl->size_;
            return rudata;
        }
        case aio::etWrite:
        {
            std::map<UDTSOCKET, detail::SocketUserData>::iterator
                ib = m_impl->socket_user_data_.find(static_cast<UDTSocketImpl*>(socket->impl())->udtHandle);
            if (ib == m_impl->socket_user_data_.end() || ib->second.write_data == boost::none)
                return NULL;
            void* rudata = ib->second.write_data.get();
            int remain_event_type = UDT_EPOLL_ERR;
            ib->second.write_data = boost::none;
            if (ib->second.read_data == boost::none)
            {
                ib->second.deleted = true;
                m_impl->reclaim_list_.push_back(ib);
            }
            else
            {
                remain_event_type = UDT_EPOLL_IN;
            }
            m_impl->RemoveFromEpoll(static_cast<UDTSocketImpl*>(socket->impl())->udtHandle, remain_event_type);
            --m_impl->size_;
            return rudata;
        }
        default: Q_ASSERT(0); return NULL;
    }
}

size_t UdtPollSet::size() const
{
    return m_impl->size_;
}

void* UdtPollSet::getUserData(UdtSocket* socket, aio::EventType eventType) const
{
    assert(socket);
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
    m_impl->ReclaimSocket();
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
        m_impl->RemovePhantomSocket(&m_impl->udt_read_set_);
        m_impl->RemovePhantomSocket(&m_impl->udt_write_set_);
        return ret;
    }
}

UdtPollSet::const_iterator UdtPollSet::begin() const
{
    Q_ASSERT(m_impl);
    return const_iterator(m_impl.get(), false);
}

UdtPollSet::const_iterator UdtPollSet::end() const
{
    Q_ASSERT(m_impl);
    return const_iterator(m_impl.get(), true);
}
