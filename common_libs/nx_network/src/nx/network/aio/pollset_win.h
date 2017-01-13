#ifdef _WIN32

#pragma once

#include "abstract_pollset.h"

namespace nx {
namespace network {
namespace aio {
namespace win32 {

class ConstIteratorImpl;

class PollSetIterator:
    public AbstractPollSetIterator
{
public:
    PollSetIterator();

    virtual bool next() override;

    virtual Pollable* socket() override;
    virtual const Pollable* socket() const override;
    virtual aio::EventType eventReceived() const override;
    virtual void* userData() override;

    ConstIteratorImpl* impl();

private:
    ConstIteratorImpl* m_impl;
};

class PollSetImpl;

class NX_NETWORK_API PollSet:
    public AbstractPollSet
{
public:
    PollSet();
    virtual ~PollSet() override;

    virtual bool isValid() const override;
    virtual void interrupt() override;
    virtual bool add(Pollable* const sock, EventType eventType, void* userData = NULL) override;
    virtual void remove(Pollable* const sock, EventType eventType) override;
    virtual size_t size() const override;
    virtual int poll(int millisToWait = kInfiniteTimeout) override;
    virtual std::unique_ptr<AbstractPollSetIterator> getSocketEventsIterator() override;

private:
    PollSetImpl* m_impl;
};

} // namespace win32
} // namespace aio
} // namespace network
} // namespace nx

#endif // _WIN32
