#include <algorithm>
#include "mdns_consumer_data.h"


namespace detail {

bool ConsumerDataList::registerConsumer(uintptr_t consumer)
{
    QnMutexLocker lock(&m_mutex);
    bool hasThisConsumer = std::any_of(
        m_data.cbegin(),
        m_data.cend(),
        [consumer](const IdConsumerPair& pair)
        {
            return pair.first == consumer;
        });

    if (hasThisConsumer)
        return false;

    m_data.push_back(std::make_pair(consumer, std::make_shared<ConsumerData>()));
    return true;
}

void ConsumerDataList::addData(
    const QString& remoteAddress,
    const QString& localAddress,
    const QByteArray &responseData)
{
    decltype(m_data) localData;
    {
        QnMutexLocker lock(&m_mutex);
        localData = m_data;
    }

    for (auto& idConsumerPair: localData)
        idConsumerPair.second->addEntry(remoteAddress, localAddress, responseData);
}

std::shared_ptr<const ConsumerData> ConsumerDataList::data(uintptr_t id) const
{
    QnMutexLocker lock(&m_mutex);
    for (auto& idConsumerPair: m_data)
    {
        if (idConsumerPair.first == id)
            return idConsumerPair.second;
    }

    return nullptr;
}

void ConsumerDataList::clearRead()
{
    QnMutexLocker lock(&m_mutex);
    for (auto& idConsumerPair: m_data)
        idConsumerPair.second->clearIfRead();
}

} // namespace detail


const int ConsumerData::kMaxEntriesCount = 256;

void ConsumerData::forEachEntry(EntryFunc func) const
{
    // TODO: It's safer to only copy m_addrToEntries under mutex so func is called mutex free,
    // but it will affect performance.
    QnMutexLocker lock(&m_mutex);
    for (auto it = m_addrToEntries.cbegin(); it != m_addrToEntries.cend(); ++it)
    {
        const QString& remoteAddress = it.key();
        for (const auto& entry: it.value())
            func(remoteAddress, entry.localAddress, entry.responseData);
    }
    m_read = true;
}

void ConsumerData::addEntry(
    const QString& remoteAddress,
    const QString& localAddress,
    const QByteArray& responseData)
{
    QnMutexLocker lock(&m_mutex);
    RemoteAddrToEntries::iterator it = m_addrToEntries.find(remoteAddress);
    if (it == m_addrToEntries.end())
    {
        it = m_addrToEntries.insert(remoteAddress, EntryList());
        it.value().push_back(Entry(localAddress, responseData));
        return;
    }

    it.value().append(Entry(localAddress, responseData));
    while (it.value().size() > kMaxEntriesCount)
        it.value().pop_front();
}

void ConsumerData::clearIfRead()
{
    QnMutexLocker lock(&m_mutex);
    if (!m_read)
        return;

    m_addrToEntries.clear();
    m_read = false;
}


