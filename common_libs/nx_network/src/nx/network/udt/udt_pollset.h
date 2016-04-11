/**********************************************************
* Nov 25, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_NETWORK_UDT_POLLSET_H
#define NX_NETWORK_UDT_POLLSET_H

#if 0

#include <memory>

#include "../aio/pollset.h"


namespace nx {
namespace network {

namespace detail {
class UdtPollSetImpl;
class UdtPollSetConstIteratorImpl;
}

class UdtSocket;

// Udt poller 
class NX_NETWORK_API UdtPollSet
{
public:
    class const_iterator
    {
    public:
        const_iterator(detail::UdtPollSetImpl* impl, bool end);
        const_iterator(const_iterator&&);
        ~const_iterator();

        const_iterator& operator++();

        UdtSocket* socket();
        const UdtSocket* socket() const;
        nx::network::aio::EventType eventType() const;
        void* userData();

        bool operator==(const const_iterator& right) const;
        bool operator!=(const const_iterator& right) const;

    private:
        detail::UdtPollSetConstIteratorImpl* m_impl;

        const_iterator(const const_iterator&);
        const_iterator& operator=(const const_iterator&);
    };

public:
    UdtPollSet();
    ~UdtPollSet();

    bool isValid() const
    {
        return static_cast<bool>(m_impl);
    }
    void interrupt();
    bool add(UdtSocket* sock, nx::network::aio::EventType eventType, void* userData = NULL);
    void* remove(UdtSocket*  sock, nx::network::aio::EventType eventType);
    void* getUserData(UdtSocket*  sock, nx::network::aio::EventType eventType) const;
    bool canAcceptSocket(UdtSocket* sock) const { Q_UNUSED(sock); return true; }
    int poll(int millisToWait = nx::network::aio::INFINITE_TIMEOUT);
    const_iterator begin() const;
    const_iterator end() const;
    size_t size() const;
    static unsigned int maxPollSetSize()
    {
        return std::numeric_limits<unsigned int>::max();
    }

private:
    detail::UdtPollSetImpl* m_impl;
    Q_DISABLE_COPY(UdtPollSet)
};

}   //network
}   //nx

#endif

#endif  //NX_NETWORK_UDT_POLLSET_H
