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
Yunhong Gu, last updated 01/27/2011
*****************************************************************************/

#ifndef __UDT_CACHE_H__
#define __UDT_CACHE_H__

#include <list>
#include <mutex>
#include <vector>

#include "common.h"
#include "detail/lru_cache.h"
#include "socket_addresss.h"
#include "udt.h"

template<typename T>
class CCache
{
public:
    CCache(int size = 1024):
        m_cache(size)
    {
    }

public:
    // Get item by the key. If item with such key is found, it is copied to *data.
    // @return true if found a match, false otherwise.
    bool lookup(int key, T* data)
    {
        std::lock_guard<std::mutex> cacheguard(m_lock);

        auto item = m_cache.getValue(key);
        if (!item)
            return false;

        *data = *item;
        return true;
    }

    // Insert new or update existing item in the cache.
    void put(int key, T data)
    {
        std::lock_guard<std::mutex> cacheguard(m_lock);
        m_cache.put(key, std::move(data));
    }

    void clear()
    {
        std::lock_guard<std::mutex> cacheguard(m_lock);
        m_cache.clear();
    }

private:
    udt::detail::LruCache<int, T> m_cache;
    std::mutex m_lock;
};

class CInfoBlock
{
public:
    uint32_t m_piIP[4];        // IP address, machine read only, not human readable format
    int m_iIPversion = 0;        // IP version
    uint64_t m_ullTimeStamp = 0;    // last update time
    std::chrono::microseconds m_iRTT = std::chrono::microseconds::zero();            // RTT
    int m_iBandwidth = 0;        // estimated bandwidth
    int m_iLossRate = 0;        // average loss rate
    int m_iReorderDistance = 0;    // packet reordering distance
    double m_dInterval = 0.0;        // inter-packet time, congestion control
    double m_dCWnd = 0.0;        // congestion window size, congestion control

public:
    CInfoBlock();

    int getKey() const;

    bool operator==(const CInfoBlock& obj) const;

public:

    // Functionality:
    //    convert sockaddr structure to an integer array
    // Parameters:
    //    0) [in] addr: network address
    //    1) [in] ver: IP version
    //    2) [out] ip: the result machine readable IP address in integer array
    // Returned value:
    //    None.

    static void convert(const detail::SocketAddress& addr, uint32_t ip[]);
};


#endif
