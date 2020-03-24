#pragma once

#include <vector>

#include "packet.h"

class UnitQueue;

struct Unit
{
    enum class Flag
    {
        free_ = 0,
        occupied,
        outOfOrder, //< msg read but not freed
        msgDropped,
    };

    // TODO: #ak There is a dependency loop between UnitQueue and Unit.
    Unit(UnitQueue* unitQueue);

    CPacket& packet();

    void setFlag(Flag val);
    Flag flag() const;

private:
    CPacket m_Packet;
    Flag m_iFlag = Flag::free_;
    UnitQueue* m_unitQueue = nullptr;
};

//-------------------------------------------------------------------------------------------------

class UnitQueue
{
public:
    UnitQueue();
    ~UnitQueue();

    UnitQueue(const UnitQueue&) = delete;
    UnitQueue& operator=(const UnitQueue&) = delete;

public:

    // Functionality:
    //    Initialize the unit queue.
    // Parameters:
    //    1) [in] size: queue size
    //    2) [in] mss: maximum segament size
    //    3) [in] version: IP version
    // Returned value:
    //    0: success, -1: failure.

    int init(int size, int mss);

    // Functionality:
    //    Increase (double) the unit queue size.
    // Parameters:
    //    None.
    // Returned value:
    //    0: success, -1: failure.

    int increase();

    // Functionality:
    //    Decrease (halve) the unit queue size.
    // Parameters:
    //    None.
    // Returned value:
    //    0: success, -1: failure.

    int shrink();

    // Functionality:
    //    find an available unit for incoming packet.
    // Parameters:
    //    None.
    // Returned value:
    //    Pointer to the available unit, NULL if not found.

    Unit* getNextAvailUnit();

    int decCount() { return --m_iCount; }
    int incCount() { return ++m_iCount; }

private:
    struct CQEntry
    {
        // TODO: #ak It makes sense to use a single buffer of (size * m_iMSS) here.
        std::vector<Unit> unitQueue;
        CQEntry* next = nullptr;

        CQEntry(std::vector<Unit> unitQueue):
            unitQueue(std::move(unitQueue))
        {
        }
    };

    CQEntry* m_pQEntry = nullptr;            // pointer to the first unit queue
    CQEntry* m_pCurrQueue = nullptr;        // pointer to the current available queue
    CQEntry* m_pLastQueue = nullptr;        // pointer to the last unit queue

    Unit* m_pAvailUnit = nullptr;         // recent available unit

    int m_iSize = 0;            // total size of the unit queue, in number of packets
    int m_iCount = 0;        // total number of valid packets in the queue

    int m_iMSS = 0;            // unit buffer size

private:
    std::unique_ptr<CQEntry> makeEntry(std::size_t size);
};
