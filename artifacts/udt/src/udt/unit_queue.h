#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "packet.h"

class UnitQueueImpl;

struct UDT_API Unit
{
    enum class Flag
    {
        free_ = 0,
        occupied,
        outOfOrder, //< msg read but not freed
        msgDropped,
    };

    Unit(std::weak_ptr<UnitQueueImpl> unitQueue, std::unique_ptr<CPacket> packet);
    Unit(Unit&&) = default;
    ~Unit();

    CPacket& packet();

    void setFlag(Flag val);
    Flag flag() const;

private:
    std::weak_ptr<UnitQueueImpl> m_unitQueue;
    std::unique_ptr<CPacket> m_packet;
    Flag m_iFlag = Flag::free_;
};

//-------------------------------------------------------------------------------------------------

/**
 * Provides empty buffers for reading into.
 * Reuses buffers that are no longer in use.
 * The whole point of this class is to minimize memory allocation count.
 */
class UDT_API UnitQueue
{
public:
    UnitQueue(int initialQueueSize, int bufferSize);
    ~UnitQueue();

    UnitQueue(const UnitQueue&) = delete;
    UnitQueue& operator=(const UnitQueue&) = delete;

    /**
     * @return an available pre-allocated packet.
     * When the Unit object is destroyed, the packet is put back into the queue if the queue is
     * still alive. So, the user code doesn't need to worry about the relative lifetime of the
     * UnitQueue and Unit objects.
     */
    Unit takeNextAvailUnit();

    // Put packet taken previously with UnitQueue::takeNextAvailUnit back for a later reuse.
    void putBack(std::unique_ptr<CPacket> packet);

private:
    std::shared_ptr<UnitQueueImpl> m_impl;
};
