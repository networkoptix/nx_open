#pragma once

#include <cstddef>
#include <memory>

#include "event_type.h"

namespace nx {
namespace network {

class Pollable;

namespace aio {

static const int kInfiniteTimeout = -1;

class AbstractPollSetIterator
{
public:
    virtual ~AbstractPollSetIterator() = default;

    /**
     * @return true if iterator has been moved to a valid position. false otherwise.
     */
    virtual bool next() = 0;

    virtual Pollable* socket() = 0;
    virtual const Pollable* socket() const = 0;
    virtual aio::EventType eventReceived() const = 0;
};

/**
 * Wrapper on top of OS provided epoll, select or kqueue.
 * Allows to wait for state change on mutiple sockets.
 * If AbstractPollSet::poll() returns positive value, then AbstractPollSet::getSocketEventsIterator()
 *   returns iterator pointing just before the first socket which state has been changed.
 * NOTE: Every socket is always monitored for error and all errors are reported.
 * NOTE: This class is not thread-safe.
 * NOTE: If multiple event occured on same socket each event will be presented separately.
 * NOTE: Polling same socket with two PollSet instances results in undefined behavior.
 */
class AbstractPollSet
{
public:
    virtual ~AbstractPollSet() = default;

    /**
     * Returns true, if all internal data has been initialized successfully.
     */
    virtual bool isValid() const = 0;
    /**
     * Interrupts poll method, blocked in other thread.
     * This is the only method which is allowed to be called from different thread.
     * poll, called after interrupt, will return immediately.
     * But, it is unspecified whether it will return multiple times
     *   if interrupt has been called multiple times.
     */
    virtual void interrupt() = 0;
    /**
     * Add socket to set. Does not take socket ownership.
     * @param eventType event to monitor on socket sock.
     * @param userData
     * @return true, if socket added to set.
     * NOTE: This method does not check, whether sock is already in pollset.
     * NOTE: Ivalidates all iterators.
     * NOTE: userData is associated with pair (sock, eventType).
     */
    virtual bool add(Pollable* const sock, EventType eventType, void* userData = NULL) = 0;
    /**
     * Do not monitor event eventType on socket sock anymore.
     * NOTE: Ivalidates all iterators to the left of removed element.
     * So, it is ok to iterate signalled sockets and remove current element:
     *   following iterator increment operation will perform safely.
     */
    virtual void remove(Pollable* const sock, EventType eventType) = 0;
    /**
     * Returns number of sockets in pollset.
     * Returned value should only be used for comparision against size of another PollSet instance.
     */
    virtual size_t size() const = 0;
    /**
     * @param millisToWait if 0, method returns immediatly. If > 0, returns on event or after millisToWait milliseconds.
     *   If < 0, method blocks till event.
     * @return -1 on error, 0 if millisToWait timeout has expired, > 0 - number of socket whose state has been changed.
     * NOTE: If multiple event occured on same socket each event will be present as a single element.
     */
    virtual int poll(int millisToWait = kInfiniteTimeout) = 0;
    /**
     * Returns iterator pointing just before first socket event read with AbstractPollSet::poll.
     * NOTE: One has to call AbstractPollSetIterator::next() to move iterator to the first position.
     */
    virtual std::unique_ptr<AbstractPollSetIterator> getSocketEventsIterator() = 0;
};

} // namespace aio
} // namespace network
} // namespace nx
