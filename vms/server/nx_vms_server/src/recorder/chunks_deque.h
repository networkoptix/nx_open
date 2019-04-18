#pragma once

#include <deque>
#include "chunk.h"

namespace nx::vms::server {

class ChunksDeque: public std::deque<Chunk>
{
public:
    class Iterator;

    struct ProxyChunk
    {
        ProxyChunk(ChunksDeque* deque, Chunk* originalChunk) :
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

        ProxyChunk& operator=(const ProxyChunk& chunk)
        {
            deque->onChunkUpdated(*originalChunk, chunk.chunk());
            *originalChunk = chunk.chunk();
            return *this;
        }

        const Chunk& chunk() const { return *originalChunk; }
        friend bool operator<(
            const ChunksDeque::ProxyChunk& first, const ChunksDeque::ProxyChunk& second);
        friend bool operator<(const ChunksDeque::ProxyChunk& first, int64_t second);
        friend bool operator<(int64_t first, const ChunksDeque::ProxyChunk& second);

    private:
        friend class Iterator;

        ChunksDeque* deque = nullptr;
        Chunk* originalChunk = nullptr;
    };

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

        Iterator(const ChunksDeque* deque, std::deque<Chunk>::iterator originalIterator):
            Iterator(const_cast<ChunksDeque*>(deque), std::move(originalIterator))
        {}

        ProxyChunk& operator*()
        {
            m_proxyChunk = Base::operator*();
            m_proxyChunk.deque = m_deque;
            return m_proxyChunk;
        }

        const ProxyChunk& operator*() const
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

        friend Iterator operator+(const Iterator& it, std::deque<Chunk>::size_type val)
        {
            return Iterator(it.m_deque, static_cast<const Base&>(it) + val);
        }

        friend bool operator<(const Iterator& first, const Iterator& second)
        {
            return static_cast<const Base&>(first) < static_cast<const Base&>(second);
        }

    private:
        using Base = std::deque<Chunk>::iterator;

        mutable ProxyChunk m_proxyChunk;
        mutable ChunksDeque* m_deque;
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

    Iterator begin()
    {
        return Iterator(this, Base::begin());
    }

    Iterator end()
    {
        return Iterator(this, Base::end());
    }

    const_iterator begin() const
    {
        return Base::begin();
    }

    const_iterator end() const
    {
        return Base::end();
    }

    const_iterator cbegin() const { return Base::cbegin(); }
    const_iterator cend() const { return Base::cend(); }

    int64_t occupiedSpace(int storageIndex) const
    {
        return m_archivePresence[storageIndex];
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
    mutable std::unordered_map<int, int64_t> m_archivePresence;

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

struct ChunksDequeBackInserter
{
    ChunksDequeBackInserter(ChunksDeque& chunksDeque):
        m_chunksDeque(chunksDeque)
    {}

    void operator++() {}
    ChunksDequeBackInserter& operator*() { return *this; }

    ChunksDequeBackInserter& operator=(const ChunksDeque::ProxyChunk& proxyChunk)
    {
        m_chunksDeque.push_back(proxyChunk.chunk());
        return *this;
    }

    ChunksDequeBackInserter& operator=(const ChunksDequeBackInserter& other)
    {
        m_chunksDeque = other.m_chunksDeque;
        return *this;
    }

private:
    ChunksDeque& m_chunksDeque;
};

inline bool operator<(const ChunksDeque::ProxyChunk& first, const ChunksDeque::ProxyChunk& second)
{
    return *first.originalChunk < *second.originalChunk;
}

inline bool operator<(const ChunksDeque::ProxyChunk& first, int64_t second)
{
    return *first.originalChunk < second;
}

inline bool operator<(int64_t first, const ChunksDeque::ProxyChunk& second)
{
    return first < *second.originalChunk;
}

} // namespace nx::vms::server
