#pragma once

#include <deque>
#include "chunk.h"

namespace nx::vms::server {

class ChunksDeque: public std::deque<Chunk>
{
public:
    class Iterator: public std::deque<Chunk>::iterator
    {
    public:
    };

    using iterator = std::deque<Chunk>::iterator;
    using const_iterator = std::deque<Chunk>::const_iterator;
    using Base = std::deque<Chunk>;

    iterator insert(const_iterator pos, const Chunk& chunk)
    {
        chunkAdded(chunk.storageIndex);
        return Base::insert(pos, chunk);
    }

    iterator insert(const_iterator pos, Chunk&& chunk)
    {
        chunkAdded(chunk.storageIndex);
        return Base::insert(pos, std::move(chunk));
    }

    iterator insert(iterator pos, const Chunk& chunk)
    {
        chunkAdded(chunk.storageIndex);
        return Base::insert(pos, chunk);
    }

    void push_back(const Chunk& chunk)
    {
        chunkAdded(chunk.storageIndex);
        Base::push_back(chunk);
    }

    void push_back(Chunk&& chunk)
    {
        chunkAdded(chunk.storageIndex);
        Base::push_back(std::move(chunk));
    }

    template< class... Args >
    iterator emplace(const_iterator pos, Args&&... args) = delete;

    template< class... Args >
    void emplace_front(Args&&... args) = delete;

    template< class... Args >
    Base::reference emplace_front(Args&&... args) = delete;

    template< class... Args >
    void emplace_back(Args&&... args) = delete;

    template< class... Args >
    Base::reference emplace_back(Args&&... args) = delete;

    void pop_back() = delete;
    void push_front(const Chunk& value) = delete;
    void push_front(Chunk&& value) = delete;

    void pop_front()
    {
        chunkRemoved(front().storageIndex);
        Base::pop_front();
    }

    iterator erase(iterator pos)
    {
        chunkRemoved(pos->storageIndex);
        return Base::erase(pos);
    }

    iterator erase(const_iterator pos)
    {
        chunkRemoved(pos->storageIndex);
        return Base::erase(pos);
    }

    void clear() noexcept = delete;

    iterator erase(iterator first, iterator last) = delete;
    iterator erase(const_iterator first, const_iterator last) = delete;

    void reCalcArchivePresence()
    {
        m_archivePresenceMap.clear();
        for (auto it = Base::cbegin(); it != Base::cend(); ++it)
            chunkAdded(it->storageIndex);
    }

private:
    std::map<int, int> m_archivePresenceMap;

    void chunkAdded(int storageIndex)
    {
        m_archivePresenceMap[storageIndex]++;
    }

    void chunkRemoved(int storageIndex)
    {
        auto it = m_archivePresenceMap.find(storageIndex);
        if (it == m_archivePresenceMap.cend())
            return;

        if (--it->second <= 0)
            m_archivePresenceMap.erase(it);
    }
};

} // namespace nx::vms::server
