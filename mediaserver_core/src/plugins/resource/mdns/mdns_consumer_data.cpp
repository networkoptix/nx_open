#include <algorithm>
#include "mdns_consumer_data.h"


namespace detail {

bool ConsumerDataList::registerConsumer(uintptr_t consumer)
{
    bool hasThisConsumer = std::any_of(
        m_data.cbegin(),
        m_data.cend(),
        [consumer](const IdConsumerPair& pair)
        {
            return pair.first == consumer;
        });

    if (hasThisConsumer)
        return false;

    m_data.push_back(std::make_pair(consumer, ConsumerData()));
    return true;
}

void ConsumerDataList::addData(
    const QString& remoteAddress,
    const QString& localAddress,
    const QByteArray &responseData)
{
    for (auto& idConsumerPair: m_data)
        idConsumerPair.second.addEntry(remoteAddress, localAddress, responseData);
}

const ConsumerData* ConsumerDataList::data(uintptr_t id) const
{
    for (auto& idConsumerPair: m_data)
    {
        if (idConsumerPair.first == id)
            return &idConsumerPair.second;
    }

    return nullptr;
}

void ConsumerDataList::clearRead()
{
    for (auto& idConsumerPair: m_data)
        idConsumerPair.second.clearIfRead();
}

} // namespace detail


const int ConsumerData::kMaxEntriesCount = 256;

void ConsumerData::forEachEntry(EntryFunc func) const
{
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
    if (!m_read)
        return;

    m_addrToEntries.clear();
    m_read = false;
}


