#pragma once

#include <deque>
#include <unordered_map>

#include "chunk.h"

namespace nx::vms::server {

class ChunksDeque
{
public:
    struct Presence
    {
        int64_t space = 0;
        std::chrono::milliseconds duration{};
    };

    using ConstIterator = std::deque<Chunk>::const_iterator;
    using ConstReverseIterator = std::deque<Chunk>::const_reverse_iterator;

    ConstIterator insert(ConstIterator pos, const Chunk& chunk);
    void push_back(const Chunk& chunk);
    const Chunk& operator[](size_t index) const;
    const Chunk& front() const;
    const Chunk& back() const;
    const Chunk& at(size_t index) const;

    ConstIterator begin() const;
    ConstIterator end() const;

    ConstReverseIterator rbegin() const;
    ConstReverseIterator rend() const;

    ConstIterator cbegin() const;
    ConstIterator cend() const;

    int64_t occupiedSpace(int storageIndex = -1) const;
    std::chrono::milliseconds occupiedDuration(int storageIndex = -1) const;
    const std::unordered_map<int, Presence>& archivePresence() const;

    void pop_front();
    ConstIterator erase(ConstIterator pos);
    void clear();
    size_t size() const;
    void assign(ConstIterator begin, ConstIterator end);
    void swap(ChunksDeque& other);
    void resize(size_t size);
    bool empty() const;
    const std::deque<Chunk>& chunks() const { return m_deque; }

    void update(int index, const Chunk& value);
    void remove_if(std::function<bool (const Chunk&)> condition);
    void set_union(ConstIterator begin, ConstIterator end);
private:
    mutable std::unordered_map<int, Presence> m_archivePresence;
    std::deque<Chunk> m_deque;

    void chunkAdded(const Chunk& chunk) const;
    void chunkRemoved(const Chunk& chunk) const;
    void allRemoved() const;
    void allAdded() const;
    void onChunkUpdated(const Chunk& oldValue, const Chunk& newValue) const;
};

} // namespace nx::vms::server
