/*****************************************************************************
Copyright (c) 2001 - 2011, The Board of Trustees of the University of Illinois.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the
above copyright notice, this list of conditions
and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the University of Illinois
nor the names of its contributors may be used to
endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/*****************************************************************************
written by
Yunhong Gu, last updated 03/12/2011
*****************************************************************************/

#include "connection_buffer.h"

#include <cstring>
#include <cmath>

using namespace std;

SendBuffer::SendBuffer(int size, int mss):
    m_pBlock(NULL),
    m_pFirstBlock(NULL),
    m_pCurrBlock(NULL),
    m_pLastBlock(NULL),
    m_pBuffer(NULL),
    m_iNextMsgNo(1),
    m_iSize(size),
    m_iMSS(mss),
    m_iCount(0)
{
    // initial physical buffer of "size"
    m_pBuffer = new BufferNode;
    m_pBuffer->m_pcData = new char[m_iSize * m_iMSS];
    m_pBuffer->m_iSize = m_iSize;
    m_pBuffer->m_pNext = NULL;

    // circular linked list for out bound packets
    m_pBlock = new Block;
    Block* pb = m_pBlock;
    for (int i = 1; i < m_iSize; ++i)
    {
        pb->m_pNext = new Block;
        pb->m_iMsgNo = 0;
        pb = pb->m_pNext;
    }
    pb->m_pNext = m_pBlock;

    pb = m_pBlock;
    char* pc = m_pBuffer->m_pcData;
    for (int i = 0; i < m_iSize; ++i)
    {
        pb->m_pcData = pc;
        pb = pb->m_pNext;
        pc += m_iMSS;
    }

    m_pFirstBlock = m_pCurrBlock = m_pLastBlock = m_pBlock;
}

SendBuffer::~SendBuffer()
{
    Block* pb = m_pBlock->m_pNext;
    while (pb != m_pBlock)
    {
        Block* temp = pb;
        pb = pb->m_pNext;
        delete temp;
    }
    delete m_pBlock;

    while (m_pBuffer != NULL)
    {
        BufferNode* temp = m_pBuffer;
        m_pBuffer = m_pBuffer->m_pNext;
        delete[] temp->m_pcData;
        delete temp;
    }
}

void SendBuffer::addBuffer(const char* data, int len, std::chrono::milliseconds ttl, bool order)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int size = len / m_iMSS;
    if ((len % m_iMSS) != 0)
        size++;

    // dynamically increase sender buffer
    while (size + m_iCount >= m_iSize)
        increase();

    const auto time = CTimer::getTime();
    int32_t inorder = order;
    inorder <<= 29;

    Block* s = m_pLastBlock;
    for (int i = 0; i < size; ++i)
    {
        int pktlen = len - i * m_iMSS;
        if (pktlen > m_iMSS)
            pktlen = m_iMSS;

        memcpy(s->m_pcData, data + i * m_iMSS, pktlen);
        s->m_iLength = pktlen;

        s->m_iMsgNo = m_iNextMsgNo | inorder;
        if (i == 0)
            s->m_iMsgNo |= 0x80000000;
        if (i == size - 1)
            s->m_iMsgNo |= 0x40000000;

        s->m_OriginTime = time;
        s->m_iTTL = ttl;

        s = s->m_pNext;
    }
    m_pLastBlock = s;

    m_iCount += size;

    m_iNextMsgNo++;
    if (m_iNextMsgNo == CMsgNo::m_iMaxMsgNo)
        m_iNextMsgNo = 1;
}

int SendBuffer::addBufferFromFile(fstream& ifs, int len)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int size = len / m_iMSS;
    if ((len % m_iMSS) != 0)
        size++;

    // dynamically increase sender buffer
    while (size + m_iCount >= m_iSize)
        increase();

    Block* s = m_pLastBlock;
    int total = 0;
    for (int i = 0; i < size; ++i)
    {
        if (ifs.bad() || ifs.fail() || ifs.eof())
            break;

        int pktlen = len - i * m_iMSS;
        if (pktlen > m_iMSS)
            pktlen = m_iMSS;

        ifs.read(s->m_pcData, pktlen);
        if ((pktlen = ifs.gcount()) <= 0)
            break;

        // currently file transfer is only available in streaming mode, message is always in order, ttl = infinite
        s->m_iMsgNo = m_iNextMsgNo | 0x20000000;
        if (i == 0)
            s->m_iMsgNo |= 0x80000000;
        if (i == size - 1)
            s->m_iMsgNo |= 0x40000000;

        s->m_iLength = pktlen;
        s->m_iTTL = std::chrono::milliseconds(-1);
        s = s->m_pNext;

        total += pktlen;
    }
    m_pLastBlock = s;

    m_iCount += size;

    m_iNextMsgNo++;
    if (m_iNextMsgNo == CMsgNo::m_iMaxMsgNo)
        m_iNextMsgNo = 1;

    return total;
}

std::optional<Buffer> SendBuffer::readData(int32_t& msgno)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // No data to read
    if (m_pCurrBlock == m_pLastBlock)
        return std::nullopt;

    std::optional<Buffer> data;
    // TODO: #ak Avoid copying here by switching Block::m_pcData to Buffer.
    if (m_pCurrBlock->m_iLength >= 0)
        data = Buffer(m_pCurrBlock->m_pcData, m_pCurrBlock->m_iLength);

    msgno = m_pCurrBlock->m_iMsgNo;

    m_pCurrBlock = m_pCurrBlock->m_pNext;

    return data;
}

std::optional<Buffer> SendBuffer::readData(const int offset, int32_t& msgno, int& msglen)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    Block* p = m_pFirstBlock;

    for (int i = 0; i < offset; ++i)
        p = p->m_pNext;

    if ((p->m_iTTL >= std::chrono::milliseconds::zero()) &&
        ((CTimer::getTime() - p->m_OriginTime) > p->m_iTTL))
    {
        msgno = p->m_iMsgNo & 0x1FFFFFFF;

        msglen = 1;
        p = p->m_pNext;
        bool move = false;
        while (msgno == (p->m_iMsgNo & 0x1FFFFFFF))
        {
            if (p == m_pCurrBlock)
                move = true;
            p = p->m_pNext;
            if (move)
                m_pCurrBlock = p;
            msglen++;
        }

        return std::nullopt;
    }

    // TODO: #ak Avoid copying here by switching Block::m_pcData to Buffer.
    auto data = Buffer(p->m_pcData, p->m_iLength);
    msgno = p->m_iMsgNo;

    return data;
}

void SendBuffer::ackData(int offset)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (int i = 0; i < offset; ++i)
        m_pFirstBlock = m_pFirstBlock->m_pNext;

    m_iCount -= offset;

    CTimer::triggerEvent();
}

int SendBuffer::getCurrBufSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return m_iCount;
}

void SendBuffer::increase()
{
    int unitsize = m_pBuffer->m_iSize;

    // new physical buffer
    BufferNode* nbuf = new BufferNode;
    nbuf->m_pcData = new char[unitsize * m_iMSS];
    nbuf->m_iSize = unitsize;
    nbuf->m_pNext = NULL;

    // insert the buffer at the end of the buffer list
    BufferNode* p = m_pBuffer;
    while (NULL != p->m_pNext)
        p = p->m_pNext;
    p->m_pNext = nbuf;

    // new packet blocks
    Block* nblk = new Block;

    Block* pb = nblk;
    for (int i = 1; i < unitsize; ++i)
    {
        pb->m_pNext = new Block;
        pb = pb->m_pNext;
    }

    // insert the new blocks onto the existing one
    pb->m_pNext = m_pLastBlock->m_pNext;
    m_pLastBlock->m_pNext = nblk;

    pb = nblk;
    char* pc = nbuf->m_pcData;
    for (int i = 0; i < unitsize; ++i)
    {
        pb->m_pcData = pc;
        pb = pb->m_pNext;
        pc += m_iMSS;
    }

    m_iSize += unitsize;
}

////////////////////////////////////////////////////////////////////////////////

ReceiveBuffer::ReceiveBuffer(int bufsize):
    m_iSize(bufsize),
    m_iStartPos(0),
    m_iLastAckPos(0),
    m_iMaxPos(0),
    m_iNotch(0)
{
    m_pUnit.resize(m_iSize);
}

ReceiveBuffer::~ReceiveBuffer() = default;

bool ReceiveBuffer::addData(std::shared_ptr<Unit> unit, int offset)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int pos = (m_iLastAckPos + offset) % m_iSize;
    if (offset > m_iMaxPos)
        m_iMaxPos = offset;

    if (NULL != m_pUnit[pos])
        return false;

    m_pUnit[pos] = unit;

    unit->setFlag(Unit::Flag::occupied);

    return true;
}

int ReceiveBuffer::readBuffer(char* data, int len)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int p = m_iStartPos;
    int lastack = m_iLastAckPos;
    int rs = len;

    while ((p != lastack) && (rs > 0) && m_pUnit[p] != NULL)
    {
        int unitsize = m_pUnit[p]->packet().getLength() - m_iNotch;
        if (unitsize > rs)
            unitsize = rs;

        memcpy(data, m_pUnit[p]->packet().payload().data() + m_iNotch, unitsize);
        data += unitsize;

        if ((rs > unitsize) || (rs == m_pUnit[p]->packet().getLength() - m_iNotch))
        {
            m_pUnit[p] = nullptr;

            if (++p == m_iSize)
                p = 0;

            m_iNotch = 0;
        }
        else
            m_iNotch += rs;

        rs -= unitsize;
    }

    m_iStartPos = p;
    return len - rs;
}

int ReceiveBuffer::readBufferToFile(fstream& ofs, int len)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int p = m_iStartPos;
    int lastack = m_iLastAckPos;
    int rs = len;

    while ((p != lastack) && (rs > 0) && (m_pUnit[p] != NULL))
    {
        int unitsize = m_pUnit[p]->packet().getLength() - m_iNotch;
        if (unitsize > rs)
            unitsize = rs;

        ofs.write(m_pUnit[p]->packet().payload().data() + m_iNotch, unitsize);
        if (ofs.fail())
            break;

        if ((rs > unitsize) || (rs == m_pUnit[p]->packet().getLength() - m_iNotch))
        {
            m_pUnit[p] = nullptr;

            if (++p == m_iSize)
                p = 0;

            m_iNotch = 0;
        }
        else
            m_iNotch += rs;

        rs -= unitsize;
    }

    m_iStartPos = p;

    return len - rs;
}

void ReceiveBuffer::ackData(int len)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_iLastAckPos = (m_iLastAckPos + len) % m_iSize;
    m_iMaxPos -= len;
    if (m_iMaxPos < 0)
        m_iMaxPos = 0;

    CTimer::triggerEvent();
}

int ReceiveBuffer::getAvailBufSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // One slot must be empty in order to tell the difference between "empty buffer" and "full buffer"
    return m_iSize - getRcvDataSize(lock) - 1;
}

int ReceiveBuffer::getRcvDataSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return getRcvDataSize(lock);
}

int ReceiveBuffer::getRcvDataSize(const std::lock_guard<std::mutex>&) const
{
    if (m_iLastAckPos >= m_iStartPos)
        return m_iLastAckPos - m_iStartPos;

    return m_iSize + m_iLastAckPos - m_iStartPos;
}

void ReceiveBuffer::dropMsg(int32_t msgno)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (int i = m_iStartPos, n = (m_iLastAckPos + m_iMaxPos) % m_iSize; i != n; i = (i + 1) % m_iSize)
        if ((NULL != m_pUnit[i]) && (msgno == m_pUnit[i]->packet().m_iMsgNo))
            m_pUnit[i]->setFlag(Unit::Flag::msgDropped);
}

int ReceiveBuffer::readMsg(char* data, int len)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int p, q;
    bool passack;
    if (!scanMsg(lock, p, q, passack))
        return 0;

    int rs = len;
    while ((p != (q + 1) % m_iSize) && m_pUnit[p] != NULL)
    {
        int unitsize = m_pUnit[p]->packet().getLength();
        if ((rs >= 0) && (unitsize > rs))
            unitsize = rs;

        if (unitsize > 0)
        {
            memcpy(data, m_pUnit[p]->packet().payload().data(), unitsize);
            data += unitsize;
            rs -= unitsize;
        }

        if (!passack)
            m_pUnit[p] = nullptr;
        else
            m_pUnit[p]->setFlag(Unit::Flag::outOfOrder);

        if (++p == m_iSize)
            p = 0;
    }

    if (!passack)
        m_iStartPos = (q + 1) % m_iSize;

    return len - rs;
}

int ReceiveBuffer::getRcvMsgNum()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    int p, q;
    bool passack;
    return scanMsg(lock, p, q, passack) ? 1 : 0;
}

bool ReceiveBuffer::scanMsg(
    const std::lock_guard<std::mutex>& lock,
    int& p, int& q, bool& passack)
{
    // empty buffer
    if ((m_iStartPos == m_iLastAckPos) && (m_iMaxPos <= 0))
        return false;

    //skip all bad msgs at the beginning
    while (m_iStartPos != m_iLastAckPos)
    {
        if (NULL == m_pUnit[m_iStartPos])
        {
            if (++m_iStartPos == m_iSize)
                m_iStartPos = 0;
            continue;
        }

        if ((Unit::Flag::occupied == m_pUnit[m_iStartPos]->flag()) && (m_pUnit[m_iStartPos]->packet().getMsgBoundary() > 1))
        {
            bool good = true;

            // look ahead for the whole message
            for (int i = m_iStartPos; i != m_iLastAckPos;)
            {
                if ((NULL == m_pUnit[i]) || (Unit::Flag::occupied != m_pUnit[i]->flag()))
                {
                    good = false;
                    break;
                }

                if ((m_pUnit[i]->packet().getMsgBoundary() == 1) || (m_pUnit[i]->packet().getMsgBoundary() == 3))
                    break;

                if (++i == m_iSize)
                    i = 0;
            }

            if (good)
                break;
        }

        m_pUnit[m_iStartPos] = nullptr;

        if (++m_iStartPos == m_iSize)
            m_iStartPos = 0;
    }

    p = -1;                  // message head
    q = m_iStartPos;         // message tail
    passack = m_iStartPos == m_iLastAckPos;
    bool found = false;

    // looking for the first message
    for (int i = 0, n = m_iMaxPos + getRcvDataSize(lock); i <= n; ++i)
    {
        if ((NULL != m_pUnit[q]) && (m_pUnit[q]->flag() == Unit::Flag::occupied))
        {
            switch (m_pUnit[q]->packet().getMsgBoundary())
            {
                case 3: // 11
                    p = q;
                    found = true;
                    break;

                case 2: // 10
                    p = q;
                    break;

                case 1: // 01
                    if (p != -1)
                        found = true;
            }
        }
        else
        {
            // a hole in this message, not valid, restart search
            p = -1;
        }

        if (found)
        {
            // the msg has to be ack'ed or it is allowed to read out of order, and was not read before
            if (!passack || !m_pUnit[q]->packet().getMsgOrderFlag())
                break;

            found = false;
        }

        if (++q == m_iSize)
            q = 0;

        if (q == m_iLastAckPos)
            passack = true;
    }

    // no msg found
    if (!found)
    {
        // if the message is larger than the receiver buffer, return part of the message
        if ((p != -1) && ((q + 1) % m_iSize == p))
            found = true;
    }

    return found;
}
