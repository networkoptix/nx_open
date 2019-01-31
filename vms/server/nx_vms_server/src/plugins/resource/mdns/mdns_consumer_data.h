#pragma once

#include <memory>
#include <stdint.h>
#include <chrono>
#include <list>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>

#include "mdns_consumer_data.h"

class ConsumerData
{
public:
    static const int kMaxEntriesCount;
    using EntryFunc = nx::utils::MoveOnlyFunc<void(
        const QString& /*remoteAddr*/,
        const QString& /*localAddr*/,
        const QByteArray& /*responseData*/)>;

    void forEachEntry(EntryFunc func) const;
    void addEntry(const QString& remoteAddress,
        const QString& localAddress,
        const QByteArray& responseData);
    void clearIfRead();

private:
    struct Entry
    {
        QString localAddress;
        QByteArray responseData;

        Entry(const QString& localAddress, const QByteArray& responseData):
            localAddress(localAddress),
            responseData(responseData)
        {}
    };
    using EntryList = QList<Entry>;
    using RemoteAddrToEntries = QMap<QString, EntryList>;

    mutable QnMutex m_mutex;
    RemoteAddrToEntries m_addrToEntries;
    mutable bool m_read = false;
};

namespace detail {

class ConsumerDataList
{
public:
    bool registerConsumer(uintptr_t consumer);

    void addData(
        const QString& remoteAddress,
        const QString& localAddress,
        const QByteArray& responseData);

    std::shared_ptr<const ConsumerData> data(uintptr_t id) const;
    void clearRead();

private:
    /** List is required to guarantee, that consumers receive data in order they were registered. */
    using IdConsumerPair = std::pair<std::uintptr_t, std::shared_ptr<ConsumerData>>;
    using IdConsumerDataList = std::list<IdConsumerPair>;

    mutable QnMutex m_mutex;
    IdConsumerDataList m_data;
};

using ConsumerDataListPtr = std::unique_ptr<ConsumerDataList>;

} // namespace detail
