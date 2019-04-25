#pragma once

#include <deque>
#include "chunk.h"

namespace nx::vms::server {

class ChunksDeque
{
public:
    class Iterator;

    struct ProxyChunk
    {
        ProxyChunk(const ChunksDeque* deque, const Chunk* originalChunkPtr) :
            deque(deque),
            originalChunkPtr(const_cast<Chunk*>(originalChunkPtr)),
            originalChunk(*originalChunkPtr)
        {
        }

        ProxyChunk() = default;
        ProxyChunk(const ProxyChunk& other) = default;

        ProxyChunk& operator=(const Chunk& chunk) const = delete;
        ProxyChunk& operator=(const Chunk& chunk)
        {
            deque->onChunkUpdated(*originalChunkPtr, chunk);
            *const_cast<Chunk*>(originalChunkPtr) = chunk;
            return *this;
        }

        ProxyChunk& operator=(const ProxyChunk& chunk) const = delete;
        ProxyChunk& operator=(const ProxyChunk& chunk)
        {
            deque->onChunkUpdated(originalChunk, chunk.originalChunk);
            *const_cast<Chunk*>(originalChunkPtr) = chunk.originalChunk;
            return *this;
        }

        const Chunk& chunk() const { return originalChunk; }
        friend bool operator<(
            const ChunksDeque::ProxyChunk& first, const ChunksDeque::ProxyChunk& second);
        friend bool operator<(const ChunksDeque::ProxyChunk& first, int64_t second);
        friend bool operator<(int64_t first, const ChunksDeque::ProxyChunk& second);

        void swap(ProxyChunk& other)
        {
            Chunk tmp = originalChunk;
            *originalChunkPtr = other.originalChunk;
            *other.originalChunkPtr = tmp;

            if (deque)
            {
                deque->chunkRemoved(other.originalChunk);
                deque->chunkAdded(originalChunk);
            }

            if (other.deque)
            {
                other.deque->chunkRemoved(originalChunk);
                other.deque->chunkAdded(other.originalChunk);
            }
        }

    private:
        friend class Iterator;

        const ChunksDeque* deque = nullptr;
        Chunk* originalChunkPtr = nullptr;
        const Chunk originalChunk;
    };

    class Iterator
    {
    public:
        Iterator(const ChunksDeque* deque, std::deque<Chunk>::iterator originalIterator):
            m_it(originalIterator),
            m_deque(deque)
        {}

        Iterator(const Iterator& other) = default;
        Iterator& operator=(const Iterator& other) = default;

        ProxyChunk operator*()
        {
            return ProxyChunk(m_deque, &(*m_it));
        }

        const Chunk* const operator->() const
        {
            return &(*m_it);
        }

        Iterator& operator++()
        {
            ++m_it;
            return *this;
        }

        Iterator operator++(int)
        {
            ++m_it;
            return Iterator(m_deque, m_it - 1);
        }

        bool operator==(const Iterator& other) const
        {
            return m_it == other.m_it;
        }

        bool operator!=(const Iterator& other) const
        {
            return m_it != other.m_it;
        }

        Iterator operator+(std::deque<Chunk>::size_type val)
        {
            return Iterator(m_deque, m_it + val);
        }

        Iterator operator-(std::deque<Chunk>::size_type val)
        {
            return Iterator(m_deque, m_it - val);
        }

        std::iterator_traits<std::deque<Chunk>::iterator>::difference_type operator-(
            const Iterator& other) const
        {
            return m_it - other.m_it;
        }

        Iterator& operator+=(std::deque<Chunk>::size_type val)
        {
            m_it += val;
            return *this;
        }

        Iterator& operator-=(std::deque<Chunk>::size_type val)
        {
            m_it -= val;
            return *this;
        }

        Iterator& operator--()
        {
            --m_it;
            return *this;
        }

        Iterator operator--(int)
        {
            --m_it;
            return Iterator(m_deque, m_it + 1);
        }

        bool operator<(const Iterator& other) const
        {
            return m_it < other.m_it;
        }

        bool operator>(const Iterator& other) const
        {
            return m_it > other.m_it;
        }

        bool operator<=(const Iterator& other) const
        {
            return m_it <= other.m_it;
        }

        bool operator>=(const Iterator& other) const
        {
            return m_it >= other.m_it;
        }

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::iterator m_it;
        const ChunksDeque* m_deque;
    };

    class ReverseIterator
    {
    public:
        ReverseIterator(
            const ChunksDeque* deque, std::deque<Chunk>::reverse_iterator originalIterator)
            :
            m_it(originalIterator),
            m_deque(deque)
        {}

        ReverseIterator(const ReverseIterator& other) = default;
        ReverseIterator& operator=(const ReverseIterator& other) = default;

        ProxyChunk operator*()
        {
            return ProxyChunk(m_deque, &(*m_it));
        }

        const Chunk* const operator->() const
        {
            return &(*m_it);
        }

        ReverseIterator& operator++()
        {
            ++m_it;
            return *this;
        }

        ReverseIterator operator++(int)
        {
            ++m_it;
            return ReverseIterator(m_deque, m_it - 1);
        }

        bool operator==(const ReverseIterator& other) const
        {
            return m_it == other.m_it;
        }

        bool operator!=(const ReverseIterator& other) const
        {
            return m_it != other.m_it;
        }

        ReverseIterator operator+(std::deque<Chunk>::size_type val)
        {
            return ReverseIterator(m_deque, m_it + val);
        }

        ReverseIterator operator-(std::deque<Chunk>::size_type val)
        {
            return ReverseIterator(m_deque, m_it - val);
        }

        std::iterator_traits<std::deque<Chunk>::reverse_iterator>::difference_type operator-(
            const ReverseIterator& other) const
        {
            return m_it - other.m_it;
        }

        ReverseIterator& operator+=(std::deque<Chunk>::size_type val)
        {
            m_it += val;
            return *this;
        }

        ReverseIterator& operator-=(std::deque<Chunk>::size_type val)
        {
            m_it -= val;
            return *this;
        }

        ReverseIterator& operator--()
        {
            --m_it;
            return *this;
        }

        ReverseIterator operator--(int)
        {
            --m_it;
            return ReverseIterator(m_deque, m_it + 1);
        }

        bool operator<(const ReverseIterator& other) const
        {
            return m_it < other.m_it;
        }

        bool operator>(const ReverseIterator& other) const
        {
            return m_it > other.m_it;
        }

        bool operator<=(const ReverseIterator& other) const
        {
            return m_it <= other.m_it;
        }

        bool operator>=(const ReverseIterator& other) const
        {
            return m_it >= other.m_it;
        }

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::reverse_iterator m_it;
        const ChunksDeque* m_deque;
    };

    class ConstReverseIterator
    {
    public:
        ConstReverseIterator(
            const ChunksDeque* deque, std::deque<Chunk>::const_reverse_iterator originalIterator)
            :
            m_it(originalIterator),
            m_deque(deque)
        {}

        ConstReverseIterator(const ConstReverseIterator& other) = default;
        ConstReverseIterator& operator=(const ConstReverseIterator& other) = default;

        ProxyChunk operator*()
        {
            return ProxyChunk(m_deque, &(*m_it));
        }

        const Chunk* const operator->() const
        {
            return &(*m_it);
        }

        ConstReverseIterator& operator++()
        {
            ++m_it;
            return *this;
        }

        ConstReverseIterator operator++(int)
        {
            ++m_it;
            return ConstReverseIterator(m_deque, m_it - 1);
        }

        bool operator==(const ConstReverseIterator& other) const
        {
            return m_it == other.m_it;
        }

        bool operator!=(const ConstReverseIterator& other) const
        {
            return m_it != other.m_it;
        }

        ConstReverseIterator operator+(std::deque<Chunk>::size_type val)
        {
            return ConstReverseIterator(m_deque, m_it + val);
        }

        ConstReverseIterator operator-(std::deque<Chunk>::size_type val)
        {
            return ConstReverseIterator(m_deque, m_it - val);
        }

        std::iterator_traits<std::deque<Chunk>::const_reverse_iterator>::difference_type operator-(
            const ConstReverseIterator& other) const
        {
            return m_it - other.m_it;
        }

        ConstReverseIterator& operator+=(std::deque<Chunk>::size_type val)
        {
            m_it += val;
            return *this;
        }

        ConstReverseIterator& operator-=(std::deque<Chunk>::size_type val)
        {
            m_it -= val;
            return *this;
        }

        ConstReverseIterator& operator--()
        {
            --m_it;
            return *this;
        }

        ConstReverseIterator operator--(int)
        {
            --m_it;
            return ConstReverseIterator(m_deque, m_it + 1);
        }

        bool operator<(const ConstReverseIterator& other) const
        {
            return m_it < other.m_it;
        }

        bool operator>(const ConstReverseIterator& other) const
        {
            return m_it > other.m_it;
        }

        bool operator<=(const ConstReverseIterator& other) const
        {
            return m_it <= other.m_it;
        }

        bool operator>=(const ConstReverseIterator& other) const
        {
            return m_it >= other.m_it;
        }

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::const_reverse_iterator m_it;
        const ChunksDeque* m_deque;
    };

    class ConstIterator
    {
    public:
        ConstIterator(const ChunksDeque* deque, std::deque<Chunk>::const_iterator originalIterator):
            m_it(originalIterator),
            m_deque(deque)
        {}

        ConstIterator(const ConstIterator& other) = default;
        ConstIterator& operator=(const ConstIterator& other) = default;

        const ProxyChunk operator*() const
        {
            return ProxyChunk(m_deque, &(*m_it));
        }

        const Chunk* const operator->() const
        {
            return &(*m_it);
        }

        ConstIterator& operator++()
        {
            ++m_it;
            return *this;
        }

        ConstIterator operator++(int)
        {
            ++m_it;
            return ConstIterator(m_deque, m_it - 1);
        }

        bool operator==(const ConstIterator& other) const
        {
            return m_it == other.m_it;
        }

        bool operator!=(const ConstIterator& other) const
        {
            return m_it != other.m_it;
        }

        ConstIterator operator+(std::deque<Chunk>::size_type val)
        {
            return ConstIterator(m_deque, m_it + val);
        }

        ConstIterator operator-(std::deque<Chunk>::size_type val)
        {
            return ConstIterator(m_deque, m_it - val);
        }

        std::iterator_traits<std::deque<Chunk>::const_iterator>::difference_type operator-(
            const ConstIterator& other) const
        {
            return m_it - other.m_it;
        }

        ConstIterator& operator+=(std::deque<Chunk>::size_type val)
        {
            m_it += val;
            return *this;
        }

        ConstIterator& operator-=(std::deque<Chunk>::size_type val)
        {
            m_it -= val;
            return *this;
        }

        ConstIterator& operator--()
        {
            --m_it;
            return *this;
        }

        ConstIterator operator--(int)
        {
            --m_it;
            return ConstIterator(m_deque, m_it + 1);
        }

        bool operator<(const ConstIterator& other) const
        {
            return m_it < other.m_it;
        }

        bool operator>(const ConstIterator& other) const
        {
            return m_it > other.m_it;
        }

        bool operator<=(const ConstIterator& other) const
        {
            return m_it <= other.m_it;
        }

        bool operator>=(const ConstIterator& other) const
        {
            return m_it >= other.m_it;
        }

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::const_iterator m_it;
        const ChunksDeque* m_deque;
    };

    Iterator insert(Iterator pos, const Chunk& chunk)
    {
        chunkAdded(chunk);
        return Iterator(this, m_deque.insert(pos.m_it, chunk));
    }

    void push_back(const Chunk& chunk)
    {
        chunkAdded(chunk);
        m_deque.push_back(chunk);
    }

    ProxyChunk operator[](size_t index)
    {
        return ProxyChunk(this, &m_deque[index]);
    }

    const ProxyChunk operator[](size_t index) const
    {
        return ProxyChunk(this, &m_deque[index]);
    }

    ProxyChunk front()
    {
        return ProxyChunk(this, &m_deque.front());
    }

    const ProxyChunk front() const
    {
        return ProxyChunk(this, &m_deque.front());
    }

    ProxyChunk back()
    {
        return ProxyChunk(this, &m_deque.back());
    }

    const ProxyChunk back() const
    {
        return ProxyChunk(this, &m_deque.back());
    }

    ProxyChunk at(size_t index)
    {
        return ProxyChunk(this, &m_deque[index]);
    }

    ProxyChunk at(size_t index) const
    {
        return ProxyChunk(this, &m_deque[index]);
    }

    Iterator begin()
    {
        return Iterator(this, m_deque.begin());
    }

    ConstIterator begin() const
    {
        return ConstIterator(this, m_deque.cbegin());
    }

    ReverseIterator rbegin()
    {
        return ReverseIterator(this, m_deque.rbegin());
    }

    ConstReverseIterator rbegin() const
    {
        return ConstReverseIterator(this, m_deque.rbegin());
    }

    ReverseIterator rend()
    {
        return ReverseIterator(this, m_deque.rend());
    }

    ConstReverseIterator rend() const
    {
        return ConstReverseIterator(this, m_deque.rend());
    }

    ConstIterator cbegin() const
    {
        return ConstIterator(this, m_deque.cbegin());
    }

    ConstIterator cend() const
    {
        return ConstIterator(this, m_deque.cend());
    }

    Iterator end()
    {
        return Iterator(this, m_deque.end());
    }

    ConstIterator end() const
    {
        return ConstIterator(this, m_deque.cend());
    }

    int64_t occupiedSpace(int storageIndex) const
    {
        return m_archivePresence[storageIndex];
    }

    void pop_front()
    {
        chunkRemoved(m_deque.front());
        m_deque.pop_front();
    }

    Iterator erase(Iterator pos)
    {
        chunkRemoved((*pos).chunk());
        return Iterator(this, m_deque.erase(pos.m_it));
    }

    void clear()
    {
        allRemoved();
        m_deque.clear();
    }

    size_t size() const
    {
        return m_deque.size();
    }

    void assign(std::deque<Chunk>::iterator begin, std::deque<Chunk>::iterator end)
    {
        allRemoved();
        m_deque.assign(begin, end);
        allAdded();
    }

    void swap(ChunksDeque& other)
    {
        allRemoved();
        other.allRemoved();
        m_deque.swap(other.m_deque);
        allAdded();
        other.allAdded();
    }

    void resize(size_t size)
    {
        allRemoved();
        m_deque.resize(size);
        allAdded();
    }

    bool empty() const
    {
        return m_deque.empty();
    }

    bool operator==(const ChunksDeque& other)
    {
        return m_deque == other.m_deque;
    }

private:
    mutable std::unordered_map<int, int64_t> m_archivePresence;
    std::deque<Chunk> m_deque;

    void chunkAdded(const Chunk& chunk) const
    {
        m_archivePresence[chunk.storageIndex] += chunk.getFileSize();
    }

    void chunkRemoved(const Chunk& chunk) const
    {
        auto &oldSpaceValue = m_archivePresence[chunk.storageIndex];
        oldSpaceValue = std::max<int64_t>(0LL, oldSpaceValue - chunk.getFileSize());
    }

    void allRemoved() const
    {
        for (const auto& chunk: m_deque)
            chunkRemoved(chunk);
    }

    void allAdded() const
    {
        for (const auto& chunk: m_deque)
            chunkAdded(chunk);
    }

    void onChunkUpdated(const Chunk& oldValue, const Chunk& newValue) const
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
    return first.originalChunk < second.originalChunk;
}

inline bool operator<(const ChunksDeque::ProxyChunk& first, int64_t second)
{
    return first.originalChunk < second;
}

inline bool operator<(int64_t first, const ChunksDeque::ProxyChunk& second)
{
    return first < second.originalChunk;
}

} // namespace nx::vms::server

namespace std {

template<>
struct iterator_traits<nx::vms::server::ChunksDequeBackInserter>
{
    using value_type = nx::vms::server::Chunk;
};

template<>
struct iterator_traits<nx::vms::server::ChunksDeque::Iterator>
{
    using value_type = nx::vms::server::ChunksDeque::ProxyChunk;
    using difference_type =
        std::iterator_traits<std::deque<nx::vms::server::Chunk>::iterator>::difference_type;
    using iterator_category = std::random_access_iterator_tag;
};

template<>
struct iterator_traits<nx::vms::server::ChunksDeque::ConstIterator>
{
    using value_type = nx::vms::server::ChunksDeque::ProxyChunk;
    using difference_type =
        std::iterator_traits<std::deque<nx::vms::server::Chunk>::const_iterator>::difference_type;
    using iterator_category = std::random_access_iterator_tag;
};

template<>
inline void iter_swap<nx::vms::server::ChunksDeque::Iterator, nx::vms::server::ChunksDeque::Iterator>(
    nx::vms::server::ChunksDeque::Iterator it1, nx::vms::server::ChunksDeque::Iterator it2)
{
    auto chunk1 = *it1;
    auto chunk2 = *it2;
    chunk1.swap(chunk2);
}

} // namespace std
