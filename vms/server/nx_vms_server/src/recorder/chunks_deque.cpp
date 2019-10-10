#include "chunks_deque.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::server {

ChunksDeque::ConstIterator ChunksDeque::insert(ConstIterator pos, const Chunk& chunk)
{
    auto iterator = pos;
    chunkAdded(chunk);
    return m_deque.insert(iterator, chunk);
}

void ChunksDeque::push_back(const Chunk& chunk)
{
    chunkAdded(chunk);
    m_deque.push_back(chunk);
}

const Chunk& ChunksDeque::operator[](size_t index) const
{
    return m_deque[index];
}

const Chunk& ChunksDeque::front() const
{
    return m_deque.front();
}

const Chunk& ChunksDeque::back() const
{
    return m_deque.back();
}

int64_t ChunksDeque::occupiedSpace(int storageIndex) const
{
    if (storageIndex >= 0)
        return m_archivePresence[storageIndex].space;
    int64_t result = 0;
    for (const auto& [key, value]: m_archivePresence)
        result += value.space;
    return result;
}

std::chrono::milliseconds ChunksDeque::occupiedDuration(int storageIndex) const
{
    if (storageIndex >= 0)
        return m_archivePresence[storageIndex].duration;

    std::chrono::milliseconds result{};
    for (const auto& [key, value]: m_archivePresence)
        result += value.duration;
    return result;
}

const std::unordered_map<int, ChunksDeque::Presence>& ChunksDeque::archivePresence() const
{
    return m_archivePresence;
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

size_t ChunksDeque::size() const
{
    return m_deque.size();
}

void ChunksDeque::assign(ConstIterator begin, ConstIterator end)
{
    allRemoved();
    m_deque.assign(begin, end);
    allAdded();
}

void ChunksDeque::swap(ChunksDeque& other)
{
    allRemoved();
    other.allRemoved();
    m_deque.swap(other.m_deque);
    allAdded();
    other.allAdded();
}

void ChunksDeque::resize(size_t size)
{
    allRemoved();
    m_deque.resize(size);
    allAdded();
}

bool ChunksDeque::empty() const
{
    return m_deque.empty();
}

void ChunksDeque::update(int index, const Chunk& value)
{
    auto& chunk = m_deque[index];
    chunkRemoved(chunk);
    chunk = value;
    chunkAdded(chunk);
}

void ChunksDeque::chunkAdded(const Chunk& chunk) const
{
    auto& value = m_archivePresence[chunk.storageIndex];
    value.space += chunk.getFileSize();
    value.duration += std::chrono::milliseconds(chunk.durationMs);
}

void ChunksDeque::chunkRemoved(const Chunk& chunk) const
{
    auto &oldValue = m_archivePresence[chunk.storageIndex];
    NX_ASSERT(oldValue.space >= chunk.getFileSize());
    oldValue.space = std::max<int64_t>(0LL, oldValue.space - chunk.getFileSize());
    NX_ASSERT(oldValue.duration.count() >= chunk.durationMs);
    oldValue.duration = std::chrono::milliseconds(
        std::max<int64_t>(0LL, oldValue.duration.count() - chunk.durationMs));
}

void ChunksDeque::allRemoved() const
{
    m_archivePresence.clear();
}

void ChunksDeque::allAdded() const
{
    for (const auto& chunk: m_deque)
        chunkAdded(chunk);
}

void ChunksDeque::onChunkUpdated(const Chunk& oldValue, const Chunk& newValue) const
{
    chunkRemoved(oldValue);
    chunkAdded(newValue);
}

const Chunk& ChunksDeque::at(size_t index) const
{
    return m_deque[index];
}

ChunksDeque::ConstIterator ChunksDeque::begin() const
{
    return m_deque.begin();
}

ChunksDeque::ConstIterator ChunksDeque::end() const
{
    return m_deque.end();
}

ChunksDeque::ConstReverseIterator ChunksDeque::rbegin() const
{
    return m_deque.rbegin();
}

ChunksDeque::ConstReverseIterator ChunksDeque::rend() const
{
    return m_deque.rend();
}

ChunksDeque::ConstIterator ChunksDeque::cbegin() const
{
    return m_deque.cbegin();
}

ChunksDeque::ConstIterator ChunksDeque::cend() const
{
    return m_deque.cend();
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
    m_deque.resize(sourceData.size() + std::distance(begin, end));
    auto itr = std::set_union(sourceData.begin(), sourceData.end(), begin, end, m_deque.begin());
    m_deque.erase(itr, m_deque.end());
    allAdded();
}



} // namespace nx::vms::server

