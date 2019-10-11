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

    int64_t occupiedSpace(int storageIndex = -1) const;
    std::chrono::milliseconds occupiedDuration(int storageIndex = -1) const;
    const std::unordered_map<int, Presence>& archivePresence() const { return m_archivePresence; }

    ConstIterator insert(ConstIterator pos, const Chunk& chunk);
    void push_back(const Chunk& chunk);
    void pop_front();
    ConstIterator erase(ConstIterator pos);
    void clear();

    const Chunk& operator[](size_t index) const { return m_deque[index]; }
    const Chunk& front() const { return m_deque.front(); }
    const Chunk& back() const { return m_deque.back(); }
    const Chunk& at(size_t index) const { return m_deque[index]; }
    size_t size() const { return m_deque.size(); }
    bool empty() const { return m_deque.empty(); }
    const std::deque<Chunk>& chunks() const { return m_deque; }

    ConstIterator begin() const { return m_deque.begin(); }
    ConstIterator end() const { return m_deque.end(); }

    ConstReverseIterator rbegin() const { return m_deque.rbegin(); }
    ConstReverseIterator rend() const { return m_deque.rend(); }

    ConstIterator cbegin() const { return m_deque.cbegin(); }
    ConstIterator cend() const { return m_deque.cend(); }

    void assign(ConstIterator begin, ConstIterator end);
    void resize(size_t size);

    void update(int index, const Chunk& value);
    void update(ConstIterator itr, const Chunk& value);
    void remove_if(std::function<bool (const Chunk&)> condition);
    void set_union(ConstIterator begin, ConstIterator end);
private:
    std::unordered_map<int, Presence> m_archivePresence;
    std::deque<Chunk> m_deque;

    void chunkAdded(const Chunk& chunk);
    void chunkRemoved(const Chunk& chunk);
    void allRemoved();
    void allAdded();
};

} // namespace nx::vms::server
