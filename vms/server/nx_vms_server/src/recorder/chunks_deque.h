#pragma once

#include <deque>
#include "chunk.h"

namespace nx::vms::server {

class ChunksDeque: public std::deque<Chunk>
{
    struct ProxyChunk
    {
        ChunksDeque* deque = nullptr;
        Chunk* originalChunk = nullptr;

        ProxyChunk(ChunksDeque* deque, Chunk* originalChunk):
            deque(deque),
            originalChunk(originalChunk)
        {
        }

        ProxyChunk() = default;

        ProxyChunk& operator=(const Chunk& chunk)
        {
            deque->onChunkUpdated(*originalChunk, chunk);
            *originalChunk = chunk;
            return *this;
        }
    };

public:
    class Iterator: public std::deque<Chunk>::iterator
    {
    public:
        Iterator(ChunksDeque* deque, std::deque<Chunk>::iterator originalIterator):
            Base(std::move(originalIterator)),
            m_deque(deque)
        {
            m_proxyChunk.deque = deque;
            if (originalIterator != deque->end())
                m_proxyChunk.originalChunk = &(*originalIterator);
        }

        ProxyChunk& operator*()
        {
            m_proxyChunk = Base::operator*();
            m_proxyChunk.deque = m_deque;
            return m_proxyChunk;
        }

        Iterator operator++()
        {
            auto result = Base::operator++();
            return Iterator(m_deque, result);
        }

        Iterator operator++(int)
        {
            auto result = Base::operator++(0);
            return Iterator(m_deque, result);
        }

        friend bool operator==(const Iterator& it1, const Iterator& it2)
        {
            return static_cast<const Base&>(it1) == static_cast<const Base&>(it2);
        }

    private:
        using Base = std::deque<Chunk>::iterator;

        ProxyChunk m_proxyChunk;
        ChunksDeque* m_deque;
    };

    using const_iterator = std::deque<Chunk>::const_iterator;
    using Base = std::deque<Chunk>;

    Iterator insert(const_iterator pos, const Chunk& chunk)
    {
        chunkAdded(chunk);
        return Iterator(this, Base::insert(pos, chunk));
    }

    Iterator insert(const_iterator pos, Chunk&& chunk)
    {
        chunkAdded(chunk);
        return Iterator(this, Base::insert(pos, std::move(chunk)));
    }

    Iterator insert(iterator pos, const Chunk& chunk)
    {
        chunkAdded(chunk);
        return Iterator(this, Base::insert(pos, chunk));
    }

    void push_back(const Chunk& chunk)
    {
        chunkAdded(chunk);
        Base::push_back(chunk);
    }

    void push_back(Chunk&& chunk)
    {
        chunkAdded(chunk);
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
        chunkRemoved(front());
        Base::pop_front();
    }

    Iterator erase(iterator pos)
    {
        chunkRemoved(*pos);
        return Iterator(this, Base::erase(pos));
    }

    Iterator erase(const_iterator pos)
    {
        chunkRemoved(*pos);
        return Iterator(this, Base::erase(pos));
    }

    void clear() noexcept = delete;

    iterator erase(iterator first, iterator last) = delete;
    iterator erase(const_iterator first, const_iterator last) = delete;

private:
    std::map<int, int64_t> m_archivePresence;

    void chunkAdded(const Chunk& chunk)
    {
        m_archivePresence[chunk.storageIndex] += chunk.getFileSize();
    }

    void chunkRemoved(const Chunk& chunk)
    {
        auto &oldSpaceValue = m_archivePresence[chunk.storageIndex];
        oldSpaceValue = std::max<int64_t>(0LL, oldSpaceValue - chunk.getFileSize());
    }

    void onChunkUpdated(const Chunk& oldValue, const Chunk& newValue)
    {
        chunkRemoved(oldValue);
        chunkAdded(newValue);
    }
};

} // namespace nx::vms::server
