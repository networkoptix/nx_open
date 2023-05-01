#pragma once

#include <mutex>
#include <vector>

#include "packet.h"

class UnitQueue;

struct UDT_API Unit
{
    enum class Flag
    {
        free_ = 0,
        occupied,
        outOfOrder, //< msg read but not freed
        msgDropped,
    };

    Unit(UnitQueue*, std::unique_ptr<CPacket> packet);
    Unit(Unit&&) = default;
    ~Unit();

    CPacket& packet();

    void setFlag(Flag val);
    Flag flag() const;

private:
    UnitQueue* m_unitQueue = nullptr;
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

    // Returns an available pre-allocated packet.
    Unit takeNextAvailUnit();

    // Put packet taken previously with UnitQueue::takeNextAvailUnit back for a later reuse.
    void putBack(std::unique_ptr<CPacket> packet);

private:
    int m_bufferSize = 0;
    std::vector<std::unique_ptr<CPacket>> m_availablePackets;
    int m_takenPackets = 0;
    std::mutex m_mutex;
};
