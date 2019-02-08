#pragma once

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "abstract_pollset.h"

namespace nx {
namespace network {
namespace aio {

template<typename OldPollSetIterator>
class PollSetIteratorWrapper:
    public AbstractPollSetIterator
{
public:
    PollSetIteratorWrapper(
        OldPollSetIterator iterator,
        OldPollSetIterator endIterator)
        :
        m_iterator(std::move(iterator)),
        m_endIterator(std::move(endIterator)),
        m_beforeFirstPosition(true)
    {
    }

    virtual bool next() override
    {
        if (m_iterator == m_endIterator)
            return false;

        if (m_beforeFirstPosition)
            m_beforeFirstPosition = false;
        else
            ++m_iterator;

        return m_iterator != m_endIterator;
    }

    virtual Pollable* socket() override
    {
        return m_iterator.socket();
    }

    virtual const Pollable* socket() const override
    {
        return m_iterator.socket();
    }

    virtual aio::EventType eventReceived() const override
    {
        return m_iterator.eventType();
    }

private:
    OldPollSetIterator m_iterator;
    const OldPollSetIterator m_endIterator;
    bool m_beforeFirstPosition;
};

template<typename OldPollSetImplementation>
class PollSetWrapper:
    public AbstractPollSet
{
public:
    template<typename... Args>
    PollSetWrapper(Args&&... args):
        m_oldPollSet(std::forward<Args>(args)...)
    {
    }

    virtual bool isValid() const override
    {
        return m_oldPollSet.isValid();
    }

    virtual void interrupt() override
    {
        return m_oldPollSet.interrupt();
    }

    virtual bool add(Pollable* const sock, EventType eventType, void* userData) override
    {
        return m_oldPollSet.add(sock, eventType, userData);
    }

    virtual void remove(Pollable* const sock, EventType eventType) override
    {
        return m_oldPollSet.remove(sock, eventType);
    }

    virtual size_t size() const override
    {
        return m_oldPollSet.size();
    }

    virtual int poll(int millisToWait = kInfiniteTimeout) override
    {
        return m_oldPollSet.poll(millisToWait);
    }

    virtual std::unique_ptr<AbstractPollSetIterator> getSocketEventsIterator() override
    {
        using OldIterator = typename OldPollSetImplementation::const_iterator;

        return std::make_unique<PollSetIteratorWrapper<OldIterator>>(
            m_oldPollSet.begin(),
            m_oldPollSet.end());
    }

private:
    OldPollSetImplementation m_oldPollSet;
};

} // namespace aio
} // namespace network
} // namespace nx
