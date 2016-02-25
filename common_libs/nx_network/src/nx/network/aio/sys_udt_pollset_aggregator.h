#pragma once

#if 1

#include "pollset.h"
#include "../udt/udt_pollset.h"


namespace nx {
namespace network {
namespace aio {

class PollSetAggregatorImpl;
class UdtPollingThread;

class NX_NETWORK_API PollSetAggregator
{
public:
    class NX_NETWORK_API const_iterator
    {
        friend class PollSetAggregator;

    public:
        const_iterator();
        const_iterator( const const_iterator& );
        ~const_iterator();

        const_iterator& operator=( const const_iterator& );
        const_iterator operator++(int);    //it++
        const_iterator& operator++();       //++it

        Pollable* socket();
        const Pollable* socket() const;
        aio::EventType eventType() const;
        void* userData();

        bool operator==( const const_iterator& right ) const;
        bool operator!=( const const_iterator& right ) const;

    private:
        ConstIteratorImpl* m_impl;
    };

    PollSetAggregator();
    virtual ~PollSetAggregator();

    bool isValid() const;
    void interrupt();
    bool add(Pollable* const sock, EventType eventType, void* userData = NULL);
    void remove(Pollable* const sock, EventType eventType);
    size_t size() const;
    int poll( int millisToWait = INFINITE_TIMEOUT );

    const_iterator begin() const;
    const_iterator end() const;

private:
    PollSet m_sysPollSet;
    UdtPollSet m_udtPollSet;
    UdtPollingThread* m_udtPollingThread;

    bool add(UdtSocket* const sock, EventType eventType, void* userData = NULL);
    void remove(UdtSocket* const sock, EventType eventType);
};

}   //aio
}   //network
}   //nx

#endif
