#include "chunks_deque.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::server {

int64_t ChunksDeque::occupiedSpace(int storageIndex) const
{
    if (storageIndex < 0)
    {
        int64_t result = 0;
        for (const auto&[key, value]: m_archivePresence)
            result += value.space;
        return result;
    }
    auto itr = m_archivePresence.find(storageIndex);
    return itr != m_archivePresence.end() ? itr->second.space : 0;
}

std::chrono::milliseconds ChunksDeque::occupiedDuration(int storageIndex) const
{
    if (storageIndex < 0)
    {
        std::chrono::milliseconds result{};
        for (const auto&[key, value] : m_archivePresence)
            result += value.duration;
        return result;
    }
    auto itr = m_archivePresence.find(storageIndex);
    return itr != m_archivePresence.end() ? itr->second.duration : std::chrono::milliseconds(0);
}

ChunksDeque::ConstIterator ChunksDeque::insert(ConstIterator pos, const Chunk& chunk)
{
    chunkAdded(chunk);
    return m_deque.insert(pos, chunk);
}

void ChunksDeque::push_back(const Chunk& chunk)
{
    chunkAdded(chunk);
    m_deque.push_back(chunk);
}

void ChunksDeque::pop_front()
{
    chunkRemoved(m_deque.front());
    m_deque.pop_front();
}

ChunksDeque::ConstIterator ChunksDeque::erase(ConstIterator pos)
{
    chunkRemoved(*pos);
    return m_deque.erase(pos);
}

void ChunksDeque::clear()
{
    allRemoved();
    m_deque.clear();
}

void ChunksDeque::assign(ConstIterator begin, ConstIterator end)
{
    allRemoved();
    m_deque.assign(begin, end);
    allAdded();
}

void ChunksDeque::resize(size_t size)
{
    allRemoved();
    m_deque.resize(size);
    allAdded();
}

void ChunksDeque::update(ConstIterator itr, const Chunk& value)
{
    return update(std::distance(m_deque.cbegin(), itr), value);
}

void ChunksDeque::update(int index, const Chunk& value)
{
    auto& chunk = m_deque[index];
    chunkRemoved(chunk);
    chunk = value;
    chunkAdded(chunk);
}

void ChunksDeque::chunkAdded(const Chunk& chunk)
{
    auto& value = m_archivePresence[chunk.storageIndex];
    value.space += chunk.getFileSize();
    value.duration += std::chrono::milliseconds(chunk.durationMs);
}

void ChunksDeque::chunkRemoved(const Chunk& chunk)
{
    auto &oldValue = m_archivePresence[chunk.storageIndex];
    NX_ASSERT(oldValue.space >= chunk.getFileSize());
    oldValue.space = std::max<int64_t>(0LL, oldValue.space - chunk.getFileSize());
    NX_ASSERT(oldValue.duration.count() >= chunk.durationMs);
    oldValue.duration = std::chrono::milliseconds(
        std::max<int64_t>(0LL, oldValue.duration.count() - chunk.durationMs));
}

void ChunksDeque::allRemoved()
{
    m_archivePresence.clear();
}

void ChunksDeque::allAdded()
{
    for (const auto& chunk: m_deque)
        chunkAdded(chunk);
}

void ChunksDeque::remove_if(std::function<bool(const Chunk&)> condition)
{
    allRemoved();
    auto newEnd = std::remove_if(m_deque.begin(), m_deque.end(), condition);
    m_deque.erase(newEnd, m_deque.end());
    allAdded();
}

void ChunksDeque::set_union(ConstIterator begin, ConstIterator end)
{
    allRemoved();
    std::deque<Chunk> sourceData;
    sourceData.swap(m_deque);
    std::set_union(sourceData.begin(), sourceData.end(), begin, end, std::back_inserter(m_deque));
    allAdded();
}

} // namespace nx::vms::server
