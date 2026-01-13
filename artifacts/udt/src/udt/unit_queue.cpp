#include "unit_queue.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <utility>

class UDT_API UnitQueueImpl:
    public std::enable_shared_from_this<UnitQueueImpl>
{
public:
    UnitQueueImpl(int initialQueueSize);
    ~UnitQueueImpl();

    UnitQueueImpl(const UnitQueueImpl&) = delete;
    UnitQueueImpl& operator=(const UnitQueueImpl&) = delete;

    Unit takeNextAvailUnit();

    void putBack(std::unique_ptr<CPacket> packet);

private:
    std::deque<std::unique_ptr<CPacket>> m_availablePackets;
    std::mutex m_mutex;
};

//-------------------------------------------------------------------------------------------------

UnitQueueImpl::UnitQueueImpl(int initialQueueSize)
{
    m_availablePackets.resize(initialQueueSize);

    std::for_each(
        m_availablePackets.begin(), m_availablePackets.end(),
        [](auto& packet)
        {
            packet = std::make_unique<CPacket>();
        });
}

UnitQueueImpl::~UnitQueueImpl()
{
}

Unit UnitQueueImpl::takeNextAvailUnit()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_availablePackets.empty())
    {
        Unit unit(shared_from_this(), std::move(m_availablePackets.front()));
        m_availablePackets.pop_front();
        return unit;
    }
    else
    {
        Unit unit(shared_from_this(), std::make_unique<CPacket>());
        return unit;
    }
}

void UnitQueueImpl::putBack(std::unique_ptr<CPacket> packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_availablePackets.push_back(std::move(packet));
}

//-------------------------------------------------------------------------------------------------

Unit::Unit(std::weak_ptr<UnitQueueImpl> unitQueue, std::unique_ptr<CPacket> packet):
    m_unitQueue(unitQueue),
    m_packet(std::move(packet))
{
}

Unit::~Unit()
{
    if (!m_packet)
        return;

    if (auto unitQueueStrong = m_unitQueue.lock(); unitQueueStrong)
        unitQueueStrong->putBack(std::exchange(m_packet, nullptr));
}

std::unique_ptr<CPacket>& Unit::packet()
{
    return m_packet;
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

UnitQueue::UnitQueue(int initialQueueSize):
    m_impl(std::make_shared<UnitQueueImpl>(initialQueueSize))
{
}

UnitQueue::~UnitQueue() = default;

Unit UnitQueue::takeNextAvailUnit()
{
    return m_impl->takeNextAvailUnit();
}

void UnitQueue::putBack(std::unique_ptr<CPacket> packet)
{
    m_impl->putBack(std::move(packet));
}
