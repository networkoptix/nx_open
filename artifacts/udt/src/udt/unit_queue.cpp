#include "unit_queue.h"

#include <algorithm>
#include <cassert>
#include <utility>

Unit::Unit(UnitQueue* unitQueue, std::unique_ptr<CPacket> packet):
    m_unitQueue(unitQueue),
    m_packet(std::move(packet))
{
}

Unit::~Unit()
{
    if (m_packet)
        m_unitQueue->putBack(std::exchange(m_packet, nullptr));
}

CPacket& Unit::packet()
{
    return *m_packet;
}

void Unit::setFlag(Flag val)
{
    m_iFlag = val;
}

Unit::Flag Unit::flag() const
{
    return m_iFlag;
}

//-------------------------------------------------------------------------------------------------

UnitQueue::UnitQueue(int initialQueueSize, int bufferSize):
    m_bufferSize(bufferSize)
{
    m_availablePackets.resize(initialQueueSize);

    std::for_each(
        m_availablePackets.begin(), m_availablePackets.end(),
        [this](auto& packet)
        {
            packet = std::make_unique<CPacket>();
            packet->payload().resize(m_bufferSize);
        });
}

UnitQueue::~UnitQueue()
{
    assert(m_takenPackets == 0);
}

Unit UnitQueue::takeNextAvailUnit()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ++m_takenPackets;

    if (!m_availablePackets.empty())
    {
        Unit unit(this, std::move(m_availablePackets.back()));
        m_availablePackets.pop_back();
        return unit;
    }
    else
    {
        Unit unit(this, std::make_unique<CPacket>());
        unit.packet().payload().resize(m_bufferSize);
        return unit;
    }
}

void UnitQueue::putBack(std::unique_ptr<CPacket> packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_availablePackets.push_back(std::move(packet));

    --m_takenPackets;
}
