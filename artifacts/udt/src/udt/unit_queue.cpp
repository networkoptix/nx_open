#include "unit_queue.h"

#include <algorithm>
#include <cassert>

CPacket& Unit::packet()
{
    return m_Packet;
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
    m_availableUnits.resize(initialQueueSize);

    std::for_each(
        m_availableUnits.begin(), m_availableUnits.end(),
        [this](auto& unit)
        {
            unit = std::make_unique<Unit>();
            unit->packet().payload().resize(m_bufferSize);
        });
}

UnitQueue::~UnitQueue()
{
    assert(m_takenUnits == 0);
}

std::shared_ptr<Unit> UnitQueue::takeNextAvailUnit()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::unique_ptr<Unit> unit;
    if (!m_availableUnits.empty())
    {
        unit = std::move(m_availableUnits.back());
        m_availableUnits.pop_back();
    }
    else
    {
        unit = std::make_unique<Unit>();
        unit->packet().payload().resize(m_bufferSize);
    }

    ++m_takenUnits;

    return std::shared_ptr<Unit>(
        unit.release(),
        [this](Unit* unit) mutable
        {
            putBack(std::unique_ptr<Unit>(unit));
        });
}

void UnitQueue::putBack(std::unique_ptr<Unit> unit)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    unit->setFlag(Unit::Flag::free_);
    m_availableUnits.push_back(std::move(unit));

    --m_takenUnits;
}
