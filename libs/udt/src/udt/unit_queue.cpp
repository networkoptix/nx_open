#include "unit_queue.h"

#include <algorithm>

Unit::Unit(UnitQueue* unitQueue):
    m_unitQueue(unitQueue)
{
}

CPacket& Unit::packet()
{
    return m_Packet;
}

void Unit::setFlag(Flag val)
{
    if (m_iFlag == val)
        return;

    m_iFlag = val;

    if (m_iFlag == Flag::free_)
        m_unitQueue->decCount();
    else if (m_iFlag == Flag::occupied)
        m_unitQueue->incCount();
}

Unit::Flag Unit::flag() const
{
    return m_iFlag;
}

//-------------------------------------------------------------------------------------------------

UnitQueue::UnitQueue(int initialQueueSize, int bufferSize):
    m_iSize(initialQueueSize),
    m_iMSS(bufferSize)
{
    auto tempq = makeEntry(m_iSize).release();

    m_pQEntry = m_pCurrQueue = m_pLastQueue = tempq;
    m_pQEntry->next = m_pQEntry;

    m_pAvailUnit = &m_pCurrQueue->unitQueue[0];
}

UnitQueue::~UnitQueue()
{
    CQEntry* p = m_pQEntry;

    while (p != nullptr)
    {
        CQEntry* q = p;
        if (p == m_pLastQueue)
            p = nullptr;
        else
            p = p->next;
        delete q;
    }
}

std::shared_ptr<Unit> UnitQueue::takeNextAvailUnit()
{
    if (m_iCount * 10 > m_iSize * 9)
        increase();

    if (m_iCount >= m_iSize)
        return nullptr;

    CQEntry* entrance = m_pCurrQueue;

    do
    {
        for (Unit* sentinel = &m_pCurrQueue->unitQueue.back();
            m_pAvailUnit <= sentinel;
            ++m_pAvailUnit)
        {
            if (m_pAvailUnit->flag() == Unit::Flag::free_)
                return wrapUnitPtr(m_pAvailUnit);
        }

        if (m_pCurrQueue->unitQueue.front().flag() == Unit::Flag::free_)
        {
            m_pAvailUnit = &m_pCurrQueue->unitQueue[0];
            return wrapUnitPtr(m_pAvailUnit);
        }

        m_pCurrQueue = m_pCurrQueue->next;
        m_pAvailUnit = &m_pCurrQueue->unitQueue[0];
    } while (m_pCurrQueue != entrance);

    increase();

    return nullptr;
}

std::unique_ptr<UnitQueue::CQEntry> UnitQueue::makeEntry(std::size_t size)
{
    std::vector<Unit> unitQueue;
    unitQueue.reserve(size);
    for (std::size_t i = 0; i < size; ++i)
    {
        unitQueue.push_back(Unit(this));
        unitQueue.back().packet().payload().resize(m_iMSS);
    }

    return std::make_unique<CQEntry>(std::move(unitQueue));
}

int UnitQueue::increase()
{
    if (!m_pQEntry)
        return -1;

    // adjust/correct m_iCount
    int real_count = 0;
    CQEntry* p = m_pQEntry;
    while (p != nullptr)
    {
        real_count += (int)std::count_if(
            p->unitQueue.begin(), p->unitQueue.end(),
            [](const auto& unit) { return unit.flag() != Unit::Flag::free_; });

        if (p == m_pLastQueue)
            p = nullptr;
        else
            p = p->next;
    }
    m_iCount = real_count;
    if (double(real_count) / m_iSize < 0.9)
        return -1;

    // all queues have the same size.
    const auto size = m_pQEntry->unitQueue.size();

    auto tempq = makeEntry(size).release();

    m_pLastQueue->next = tempq;
    m_pLastQueue = tempq;
    m_pLastQueue->next = m_pQEntry;

    m_iSize += size;

    return 0;
}

std::shared_ptr<Unit> UnitQueue::wrapUnitPtr(Unit* unit)
{
    return std::shared_ptr<Unit>(
        unit,
        [](Unit* unit)
        {
            unit->setFlag(Unit::Flag::free_);
        });
}
