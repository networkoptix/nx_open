#pragma once

#include <deque>
#include <unordered_map>

#include "chunk.h"

namespace nx::vms::server {

class ChunksDeque
{
public:
    class Iterator;

    struct Presence {
        int64_t space = 0;
        std::chrono::milliseconds duration{};
    };

    struct ProxyChunk
    {
        ProxyChunk(const ChunksDeque* deque, const Chunk* originalChunkPtr);
        ProxyChunk() = default;
        ProxyChunk(const ProxyChunk& other) = default;
        ProxyChunk& operator=(const Chunk& chunk);
        ProxyChunk& operator=(const ProxyChunk& chunk);

        const Chunk& chunk() const { return originalChunk; }
        friend bool operator<(
            const ChunksDeque::ProxyChunk& first, const ChunksDeque::ProxyChunk& second);
        friend bool operator<(const ChunksDeque::ProxyChunk& first, int64_t second);
        friend bool operator<(int64_t first, const ChunksDeque::ProxyChunk& second);

        void swap(ProxyChunk& other);

    private:
        friend class Iterator;

        const ChunksDeque* deque = nullptr;
        Chunk* originalChunkPtr = nullptr;
        const Chunk originalChunk;
    };

    class Iterator
    {
    public:
        Iterator(const ChunksDeque* deque, std::deque<Chunk>::iterator originalIterator);
        Iterator(const Iterator& other) = default;
        Iterator& operator=(const Iterator& other) = default;

        ProxyChunk operator*();
        const ProxyChunk operator*() const;
        const Chunk* const operator->() const;
        Iterator& operator++();
        Iterator operator++(int);
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;
        Iterator operator+(std::deque<Chunk>::size_type val);
        Iterator operator-(std::deque<Chunk>::size_type val);
        std::iterator_traits<std::deque<Chunk>::iterator>::difference_type operator-(
            const Iterator& other) const;
        Iterator& operator+=(std::deque<Chunk>::size_type val);
        Iterator& operator-=(std::deque<Chunk>::size_type val);
        Iterator& operator--();
        Iterator operator--(int);
        bool operator<(const Iterator& other) const;
        bool operator>(const Iterator& other) const;
        bool operator<=(const Iterator& other) const;
        bool operator>=(const Iterator& other) const;

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::iterator m_it;
        const ChunksDeque* m_deque;
    };

    class ReverseIterator
    {
    public:
        ReverseIterator(
            const ChunksDeque* deque, std::deque<Chunk>::reverse_iterator originalIterator);
        ReverseIterator(const ReverseIterator& other) = default;
        ReverseIterator& operator=(const ReverseIterator& other) = default;

        ProxyChunk operator*();
        const Chunk* const operator->() const;
        ReverseIterator& operator++();
        ReverseIterator operator++(int);
        bool operator==(const ReverseIterator& other) const;
        bool operator!=(const ReverseIterator& other) const;
        ReverseIterator operator+(std::deque<Chunk>::size_type val);
        ReverseIterator operator-(std::deque<Chunk>::size_type val);
        std::iterator_traits<std::deque<Chunk>::reverse_iterator>::difference_type operator-(
            const ReverseIterator& other) const;
        ReverseIterator& operator+=(std::deque<Chunk>::size_type val);
        ReverseIterator& operator-=(std::deque<Chunk>::size_type val);
        ReverseIterator& operator--();
        ReverseIterator operator--(int);
        bool operator<(const ReverseIterator& other) const;
        bool operator>(const ReverseIterator& other) const;
        bool operator<=(const ReverseIterator& other) const;
        bool operator>=(const ReverseIterator& other) const;

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::reverse_iterator m_it;
        const ChunksDeque* m_deque;
    };

    class ConstReverseIterator
    {
    public:
        ConstReverseIterator(
            const ChunksDeque* deque, std::deque<Chunk>::const_reverse_iterator originalIterator);
        ConstReverseIterator(const ConstReverseIterator& other) = default;
        ConstReverseIterator& operator=(const ConstReverseIterator& other) = default;

        ProxyChunk operator*();
        const Chunk* const operator->() const;
        ConstReverseIterator& operator++();
        ConstReverseIterator operator++(int);
        bool operator==(const ConstReverseIterator& other) const;
        bool operator!=(const ConstReverseIterator& other) const;
        ConstReverseIterator operator+(std::deque<Chunk>::size_type val);
        ConstReverseIterator operator-(std::deque<Chunk>::size_type val);
        std::iterator_traits<std::deque<Chunk>::const_reverse_iterator>::difference_type operator-(
            const ConstReverseIterator& other) const;
        ConstReverseIterator& operator+=(std::deque<Chunk>::size_type val);
        ConstReverseIterator& operator-=(std::deque<Chunk>::size_type val);
        ConstReverseIterator& operator--();
        ConstReverseIterator operator--(int);
        bool operator<(const ConstReverseIterator& other) const;
        bool operator>(const ConstReverseIterator& other) const;
        bool operator<=(const ConstReverseIterator& other) const;
        bool operator>=(const ConstReverseIterator& other) const;

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::const_reverse_iterator m_it;
        const ChunksDeque* m_deque;
    };

    class ConstIterator
    {
    public:
        ConstIterator(const ChunksDeque* deque, std::deque<Chunk>::const_iterator originalIterator);
        ConstIterator(const ConstIterator& other) = default;
        ConstIterator& operator=(const ConstIterator& other) = default;

        const ProxyChunk operator*() const;
        const Chunk* const operator->() const;
        ConstIterator& operator++();
        ConstIterator operator++(int);
        bool operator==(const ConstIterator& other) const;
        bool operator!=(const ConstIterator& other) const;
        ConstIterator operator+(std::deque<Chunk>::size_type val);
        ConstIterator operator-(std::deque<Chunk>::size_type val);
        std::iterator_traits<std::deque<Chunk>::const_iterator>::difference_type operator-(
            const ConstIterator& other) const;
        ConstIterator& operator+=(std::deque<Chunk>::size_type val);
        ConstIterator& operator-=(std::deque<Chunk>::size_type val);
        ConstIterator& operator--();
        ConstIterator operator--(int);
        bool operator<(const ConstIterator& other) const;
        bool operator>(const ConstIterator& other) const;
        bool operator<=(const ConstIterator& other) const;
        bool operator>=(const ConstIterator& other) const;

    private:
        friend class ChunksDeque;

        std::deque<Chunk>::const_iterator m_it;
        const ChunksDeque* m_deque;
    };

    Iterator insert(Iterator pos, const Chunk& chunk);
    void push_back(const Chunk& chunk);
    ProxyChunk operator[](size_t index);
    const ProxyChunk operator[](size_t index) const;
    ProxyChunk front();
    const ProxyChunk front() const;
    ProxyChunk back();
    const ProxyChunk back() const;
    ProxyChunk at(size_t index);
    ProxyChunk at(size_t index) const;
    Iterator begin();
    ConstIterator begin() const;
    ReverseIterator rbegin();
    ConstReverseIterator rbegin() const;
    ReverseIterator rend();
    ConstReverseIterator rend() const;
    ConstIterator cbegin() const;
    ConstIterator cend() const;
    Iterator end();
    ConstIterator end() const;
    int64_t occupiedSpace(int storageIndex = -1) const;
    std::chrono::milliseconds occupiedDuration(int storageIndex = -1) const;
    const std::unordered_map<int, Presence>& archivePresence() const;
    void pop_front();
    Iterator erase(Iterator pos);
    void clear();
    size_t size() const;
    void assign(std::deque<Chunk>::iterator begin, std::deque<Chunk>::iterator end);
    void swap(ChunksDeque& other);
    void resize(size_t size);
    bool empty() const;
    bool operator==(const ChunksDeque& other);
    friend void swap(ProxyChunk c1, ProxyChunk c2) { c1.swap(c2); }
private:
    mutable std::unordered_map<int, Presence> m_archivePresence;

    std::deque<Chunk> m_deque;

    void chunkAdded(const Chunk& chunk) const;
    void chunkRemoved(const Chunk& chunk) const;
    void allRemoved() const;
    void allAdded() const;
    void onChunkUpdated(const Chunk& oldValue, const Chunk& newValue) const;
};

struct ChunksDequeBackInserter
{
    ChunksDequeBackInserter(ChunksDeque& chunksDeque);
    void operator++() {}
    ChunksDequeBackInserter& operator*() { return *this; }
    ChunksDequeBackInserter& operator=(const ChunksDeque::ProxyChunk& proxyChunk);
    ChunksDequeBackInserter& operator=(const ChunksDequeBackInserter& other);

private:
    ChunksDeque& m_chunksDeque;
};

bool operator<(const ChunksDeque::ProxyChunk& first, const ChunksDeque::ProxyChunk& second);
bool operator<(const ChunksDeque::ProxyChunk& first, int64_t second);
bool operator<(int64_t first, const ChunksDeque::ProxyChunk& second);

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
