#pragma once

#include <mutex>
#include <vector>

#include "packet.h"

struct UDT_API Unit
{
    enum class Flag
    {
        free_ = 0,
        occupied,
        outOfOrder, //< msg read but not freed
        msgDropped,
    };

    CPacket& packet();

    void setFlag(Flag val);
    Flag flag() const;

private:
    CPacket m_Packet;
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

    // Finds an available unit for incoming packet.
    // @return Pointer to the available unit, NULL if not found.
    std::shared_ptr<Unit> takeNextAvailUnit();

private:
    int m_bufferSize = 0;
    std::vector<std::unique_ptr<Unit>> m_availableUnits;
    int m_takenUnits = 0;
    std::mutex m_mutex;

    void putBack(std::unique_ptr<Unit> unit);
};
