#include "chunks_deque.h"

namespace nx::vms::server {

ChunksDeque::ProxyChunk::ProxyChunk(const ChunksDeque* deque, const Chunk* originalChunkPtr) :
    deque(deque),
    originalChunkPtr(const_cast<Chunk*>(originalChunkPtr)),
    originalChunk(*originalChunkPtr)
{
}

ChunksDeque::ProxyChunk& ChunksDeque::ProxyChunk::operator=(const Chunk& chunk)
{
    deque->onChunkUpdated(*originalChunkPtr, chunk);
    *const_cast<Chunk*>(originalChunkPtr) = chunk;
    return *this;
}

ChunksDeque::ProxyChunk& ChunksDeque::ProxyChunk::operator=(const ChunksDeque::ProxyChunk& chunk)
{
    deque->onChunkUpdated(originalChunk, chunk.originalChunk);
    *const_cast<Chunk*>(originalChunkPtr) = chunk.originalChunk;
    return *this;
}


void ChunksDeque::ProxyChunk::swap(ProxyChunk& other)
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


ChunksDeque::Iterator::Iterator(
    const ChunksDeque* deque, std::deque<Chunk>::iterator originalIterator)
    :
    m_it(originalIterator),
    m_deque(deque)
{}


ChunksDeque::ProxyChunk ChunksDeque::Iterator::operator*()
{
    return ProxyChunk(m_deque, &(*m_it));
}

const ChunksDeque::ProxyChunk ChunksDeque::Iterator::operator*() const
{
    return ProxyChunk(m_deque, &(*m_it));
}

const Chunk* const ChunksDeque::Iterator::operator->() const
{
    return &(*m_it);
}

ChunksDeque::Iterator& ChunksDeque::Iterator::operator++()
{
    ++m_it;
    return *this;
}

ChunksDeque::Iterator ChunksDeque::Iterator::operator++(int)
{
    ++m_it;
    return Iterator(m_deque, m_it - 1);
}

bool ChunksDeque::Iterator::operator==(const Iterator& other) const
{
    return m_it == other.m_it;
}

bool ChunksDeque::Iterator::operator!=(const Iterator& other) const
{
    return m_it != other.m_it;
}

ChunksDeque::Iterator ChunksDeque::Iterator::operator+(std::deque<Chunk>::size_type val)
{
    return Iterator(m_deque, m_it + val);
}

ChunksDeque::Iterator ChunksDeque::Iterator::operator-(std::deque<Chunk>::size_type val)
{
    return Iterator(m_deque, m_it - val);
}

std::iterator_traits<std::deque<Chunk>::iterator>::difference_type ChunksDeque::Iterator::operator-(
    const Iterator& other) const
{
    return m_it - other.m_it;
}

ChunksDeque::Iterator& ChunksDeque::Iterator::operator+=(std::deque<Chunk>::size_type val)
{
    m_it += val;
    return *this;
}

ChunksDeque::Iterator& ChunksDeque::Iterator::operator-=(std::deque<Chunk>::size_type val)
{
    m_it -= val;
    return *this;
}

ChunksDeque::Iterator& ChunksDeque::Iterator::operator--()
{
    --m_it;
    return *this;
}

ChunksDeque::Iterator ChunksDeque::Iterator::operator--(int)
{
    --m_it;
    return Iterator(m_deque, m_it + 1);
}

bool ChunksDeque::Iterator::operator<(const Iterator& other) const
{
    return m_it < other.m_it;
}

bool ChunksDeque::Iterator::operator>(const Iterator& other) const
{
    return m_it > other.m_it;
}

bool ChunksDeque::Iterator::operator<=(const Iterator& other) const
{
    return m_it <= other.m_it;
}

bool ChunksDeque::Iterator::operator>=(const Iterator& other) const
{
    return m_it >= other.m_it;
}

ChunksDeque::ReverseIterator::ReverseIterator(
    const ChunksDeque* deque, std::deque<Chunk>::reverse_iterator originalIterator)
    :
    m_it(originalIterator),
    m_deque(deque)
{}

ChunksDeque::ProxyChunk ChunksDeque::ReverseIterator::operator*()
{
    return ProxyChunk(m_deque, &(*m_it));
}

const Chunk* const ChunksDeque::ReverseIterator::operator->() const
{
    return &(*m_it);
}

ChunksDeque::ReverseIterator& ChunksDeque::ReverseIterator::operator++()
{
    ++m_it;
    return *this;
}

ChunksDeque::ReverseIterator ChunksDeque::ReverseIterator::operator++(int)
{
    ++m_it;
    return ReverseIterator(m_deque, m_it - 1);
}

bool ChunksDeque::ReverseIterator::operator==(const ReverseIterator& other) const
{
    return m_it == other.m_it;
}

bool ChunksDeque::ReverseIterator::operator!=(const ReverseIterator& other) const
{
    return m_it != other.m_it;
}

ChunksDeque::ReverseIterator ChunksDeque::ReverseIterator::operator+(
    std::deque<Chunk>::size_type val)
{
    return ReverseIterator(m_deque, m_it + val);
}

ChunksDeque::ReverseIterator ChunksDeque::ReverseIterator::operator-(
    std::deque<Chunk>::size_type val)
{
    return ReverseIterator(m_deque, m_it - val);
}

std::iterator_traits<std::deque<Chunk>::reverse_iterator>::difference_type ChunksDeque::ReverseIterator::operator-(
    const ReverseIterator& other) const
{
    return m_it - other.m_it;
}

ChunksDeque::ReverseIterator& ChunksDeque::ReverseIterator::operator+=(std::deque<Chunk>::size_type val)
{
    m_it += val;
    return *this;
}

ChunksDeque::ReverseIterator& ChunksDeque::ReverseIterator::operator-=(std::deque<Chunk>::size_type val)
{
    m_it -= val;
    return *this;
}

ChunksDeque::ReverseIterator& ChunksDeque::ReverseIterator::operator--()
{
    --m_it;
    return *this;
}

ChunksDeque::ReverseIterator ChunksDeque::ReverseIterator::operator--(int)
{
    --m_it;
    return ReverseIterator(m_deque, m_it + 1);
}

bool ChunksDeque::ReverseIterator::operator<(const ReverseIterator& other) const
{
    return m_it < other.m_it;
}

bool ChunksDeque::ReverseIterator::operator>(const ReverseIterator& other) const
{
    return m_it > other.m_it;
}

bool ChunksDeque::ReverseIterator::operator<=(const ReverseIterator& other) const
{
    return m_it <= other.m_it;
}

bool ChunksDeque::ReverseIterator::operator>=(const ReverseIterator& other) const
{
    return m_it >= other.m_it;
}

ChunksDeque::ConstReverseIterator::ConstReverseIterator(
    const ChunksDeque* deque, std::deque<Chunk>::const_reverse_iterator originalIterator)
    :
    m_it(originalIterator),
    m_deque(deque)
{}

ChunksDeque::ProxyChunk ChunksDeque::ConstReverseIterator::operator*()
{
    return ProxyChunk(m_deque, &(*m_it));
}

const Chunk* const ChunksDeque::ConstReverseIterator::operator->() const
{
    return &(*m_it);
}

ChunksDeque::ConstReverseIterator& ChunksDeque::ConstReverseIterator::operator++()
{
    ++m_it;
    return *this;
}

ChunksDeque::ConstReverseIterator ChunksDeque::ConstReverseIterator::operator++(int)
{
    ++m_it;
    return ConstReverseIterator(m_deque, m_it - 1);
}

bool ChunksDeque::ConstReverseIterator::operator==(const ConstReverseIterator& other) const
{
    return m_it == other.m_it;
}

bool ChunksDeque::ConstReverseIterator::operator!=(const ConstReverseIterator& other) const
{
    return m_it != other.m_it;
}

ChunksDeque::ConstReverseIterator ChunksDeque::ConstReverseIterator::operator+(
    std::deque<Chunk>::size_type val)
{
    return ConstReverseIterator(m_deque, m_it + val);
}

ChunksDeque::ConstReverseIterator ChunksDeque::ConstReverseIterator::operator-(
    std::deque<Chunk>::size_type val)
{
    return ConstReverseIterator(m_deque, m_it - val);
}

std::iterator_traits<std::deque<Chunk>::const_reverse_iterator>::difference_type ChunksDeque::ConstReverseIterator::operator-(
    const ConstReverseIterator& other) const
{
    return m_it - other.m_it;
}

ChunksDeque::ConstReverseIterator& ChunksDeque::ConstReverseIterator::operator+=(
    std::deque<Chunk>::size_type val)
{
    m_it += val;
    return *this;
}

ChunksDeque::ConstReverseIterator& ChunksDeque::ConstReverseIterator::operator-=(
    std::deque<Chunk>::size_type val)
{
    m_it -= val;
    return *this;
}

ChunksDeque::ConstReverseIterator& ChunksDeque::ConstReverseIterator::operator--()
{
    --m_it;
    return *this;
}

ChunksDeque::ConstReverseIterator ChunksDeque::ConstReverseIterator::operator--(int)
{
    --m_it;
    return ConstReverseIterator(m_deque, m_it + 1);
}

bool ChunksDeque::ConstReverseIterator::operator<(const ConstReverseIterator& other) const
{
    return m_it < other.m_it;
}

bool ChunksDeque::ConstReverseIterator::operator>(const ConstReverseIterator& other) const
{
    return m_it > other.m_it;
}

bool ChunksDeque::ConstReverseIterator::operator<=(const ConstReverseIterator& other) const
{
    return m_it <= other.m_it;
}

bool ChunksDeque::ConstReverseIterator::operator>=(const ConstReverseIterator& other) const
{
    return m_it >= other.m_it;
}

ChunksDeque::ConstIterator::ConstIterator(
    const ChunksDeque* deque, std::deque<Chunk>::const_iterator originalIterator)
    :
    m_it(originalIterator),
    m_deque(deque)
{}

const ChunksDeque::ProxyChunk ChunksDeque::ConstIterator::operator*() const
{
    return ProxyChunk(m_deque, &(*m_it));
}

const Chunk* const ChunksDeque::ConstIterator::operator->() const
{
    return &(*m_it);
}

ChunksDeque::ConstIterator& ChunksDeque::ConstIterator::operator++()
{
    ++m_it;
    return *this;
}

ChunksDeque::ConstIterator ChunksDeque::ConstIterator::operator++(int)
{
    ++m_it;
    return ConstIterator(m_deque, m_it - 1);
}

bool ChunksDeque::ConstIterator::operator==(const ConstIterator& other) const
{
    return m_it == other.m_it;
}

bool ChunksDeque::ConstIterator::operator!=(const ConstIterator& other) const
{
    return m_it != other.m_it;
}

ChunksDeque::ConstIterator ChunksDeque::ConstIterator::operator+(std::deque<Chunk>::size_type val)
{
    return ConstIterator(m_deque, m_it + val);
}

ChunksDeque::ConstIterator ChunksDeque::ConstIterator::operator-(std::deque<Chunk>::size_type val)
{
    return ConstIterator(m_deque, m_it - val);
}

std::iterator_traits<std::deque<Chunk>::const_iterator>::difference_type ChunksDeque::ConstIterator::operator-(
    const ConstIterator& other) const
{
    return m_it - other.m_it;
}

ChunksDeque::ConstIterator& ChunksDeque::ConstIterator::operator+=(
    std::deque<Chunk>::size_type val)
{
    m_it += val;
    return *this;
}

ChunksDeque::ConstIterator& ChunksDeque::ConstIterator::operator-=(
    std::deque<Chunk>::size_type val)
{
    m_it -= val;
    return *this;
}

ChunksDeque::ConstIterator& ChunksDeque::ConstIterator::operator--()
{
    --m_it;
    return *this;
}

ChunksDeque::ConstIterator ChunksDeque::ConstIterator::operator--(int)
{
    --m_it;
    return ConstIterator(m_deque, m_it + 1);
}

bool ChunksDeque::ConstIterator::operator<(const ConstIterator& other) const
{
    return m_it < other.m_it;
}

bool ChunksDeque::ConstIterator::operator>(const ConstIterator& other) const
{
    return m_it > other.m_it;
}

bool ChunksDeque::ConstIterator::operator<=(const ConstIterator& other) const
{
    return m_it <= other.m_it;
}

bool ChunksDeque::ConstIterator::operator>=(const ConstIterator& other) const
{
    return m_it >= other.m_it;
}

ChunksDeque::Iterator ChunksDeque::insert(ChunksDeque::Iterator pos, const Chunk& chunk)
{
    chunkAdded(chunk);
    return Iterator(this, m_deque.insert(pos.m_it, chunk));
}

void ChunksDeque::push_back(const Chunk& chunk)
{
    chunkAdded(chunk);
    m_deque.push_back(chunk);
}

ChunksDeque::ProxyChunk ChunksDeque::operator[](size_t index)
{
    return ProxyChunk(this, &m_deque[index]);
}

const ChunksDeque::ProxyChunk ChunksDeque::operator[](size_t index) const
{
    return ProxyChunk(this, &m_deque[index]);
}

ChunksDeque::ProxyChunk ChunksDeque::front()
{
    return ChunksDeque::ProxyChunk(this, &m_deque.front());
}

const ChunksDeque::ProxyChunk ChunksDeque::front() const
{
    return ChunksDeque::ProxyChunk(this, &m_deque.front());
}

ChunksDeque::ProxyChunk ChunksDeque::back()
{
    return ProxyChunk(this, &m_deque.back());
}

const ChunksDeque::ProxyChunk ChunksDeque::back() const
{
    return ChunksDeque::ProxyChunk(this, &m_deque.back());
}

ChunksDeque::ProxyChunk ChunksDeque::at(size_t index)
{
    return ChunksDeque::ProxyChunk(this, &m_deque[index]);
}

ChunksDeque::ProxyChunk ChunksDeque::at(size_t index) const
{
    return ProxyChunk(this, &m_deque[index]);
}

ChunksDeque::Iterator ChunksDeque::begin()
{
    return Iterator(this, m_deque.begin());
}

ChunksDeque::ConstIterator ChunksDeque::begin() const
{
    return ConstIterator(this, m_deque.cbegin());
}

ChunksDeque::ReverseIterator ChunksDeque::rbegin()
{
    return ReverseIterator(this, m_deque.rbegin());
}

ChunksDeque::ConstReverseIterator ChunksDeque::rbegin() const
{
    return ConstReverseIterator(this, m_deque.rbegin());
}

ChunksDeque::ReverseIterator ChunksDeque::rend()
{
    return ReverseIterator(this, m_deque.rend());
}

ChunksDeque::ConstReverseIterator ChunksDeque::rend() const
{
    return ConstReverseIterator(this, m_deque.rend());
}

ChunksDeque::ConstIterator ChunksDeque::cbegin() const
{
    return ConstIterator(this, m_deque.cbegin());
}

ChunksDeque::ConstIterator ChunksDeque::cend() const
{
    return ConstIterator(this, m_deque.cend());
}

ChunksDeque::Iterator ChunksDeque::end()
{
    return Iterator(this, m_deque.end());
}

ChunksDeque::ConstIterator ChunksDeque::end() const
{
    return ConstIterator(this, m_deque.cend());
}

int64_t ChunksDeque::occupiedSpace(int storageIndex) const
{
    return m_archivePresence[storageIndex];
}

void ChunksDeque::pop_front()
{
    chunkRemoved(m_deque.front());
    m_deque.pop_front();
}

ChunksDeque::Iterator ChunksDeque::erase(Iterator pos)
{
    chunkRemoved((*pos).chunk());
    return Iterator(this, m_deque.erase(pos.m_it));
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

void ChunksDeque::assign(std::deque<Chunk>::iterator begin, std::deque<Chunk>::iterator end)
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

bool ChunksDeque::operator==(const ChunksDeque& other)
{
    return m_deque == other.m_deque;
}

void ChunksDeque::chunkAdded(const Chunk& chunk) const
{
    m_archivePresence[chunk.storageIndex] += chunk.getFileSize();
}

void ChunksDeque::chunkRemoved(const Chunk& chunk) const
{
    auto &oldSpaceValue = m_archivePresence[chunk.storageIndex];
    oldSpaceValue = std::max<int64_t>(0LL, oldSpaceValue - chunk.getFileSize());
}

void ChunksDeque::allRemoved() const
{
    for (const auto& chunk: m_deque)
        chunkRemoved(chunk);
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

ChunksDequeBackInserter::ChunksDequeBackInserter(ChunksDeque& chunksDeque):
    m_chunksDeque(chunksDeque)
{}

ChunksDequeBackInserter& ChunksDequeBackInserter::operator=(
    const ChunksDeque::ProxyChunk& proxyChunk)
{
    m_chunksDeque.push_back(proxyChunk.chunk());
    return *this;
}

ChunksDequeBackInserter& ChunksDequeBackInserter::operator=(const ChunksDequeBackInserter& other)
{
    m_chunksDeque = other.m_chunksDeque;
    return *this;
}

bool operator<(const ChunksDeque::ProxyChunk& first, const ChunksDeque::ProxyChunk& second)
{
    return first.originalChunk < second.originalChunk;
}

bool operator<(const ChunksDeque::ProxyChunk& first, int64_t second)
{
    return first.originalChunk < second;
}

bool operator<(int64_t first, const ChunksDeque::ProxyChunk& second)
{
    return first < second.originalChunk;
}

} // namespace nx::vms::server

