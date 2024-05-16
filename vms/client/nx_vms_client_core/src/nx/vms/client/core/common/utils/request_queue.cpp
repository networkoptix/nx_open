// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request_queue.h"

#include <set>
#include <list>
#include <unordered_map>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/thread/mutex.h>

namespace std {

template <>
struct hash<QUuid>
{
    std::size_t operator()(const QUuid& value) const
    {
        return qHash(value);
    }
};

} // namesapce std

namespace nx::vms::client::core {

struct RequestQueue::Private
{
    using RequestHolder = std::function<void ()>;
    using RequestsContainer = std::list<std::pair<QUuid, RequestHolder>>;
    using RequestIterator = RequestsContainer::iterator;
    using IdToRequestItContainer = std::unordered_map<QUuid, RequestIterator>;

    RequestQueue* const q;
    int maxRunningCount = 1;

    std::set<QUuid> runningRequestIds;
    nx::Mutex mutex = {};
    RequestsContainer requests;
    IdToRequestItContainer idToRequestItMapping;

    QUuid addRequest(const Request& request);
    bool removeRequestUnsafe(const QUuid& id);
    bool removeRequestUnsafeInternal(const QUuid& id);
    void executeRequests();
    void clear();
};

QUuid RequestQueue::Private::addRequest(const Request& request)
{
    if (!request)
        return {};

    const auto id = QUuid::createUuid();
    auto requestHolder =
        [this, id, request]()
        {
            const auto callback = nx::utils::guarded(q,
                [this, id]()
                {
                    {
                        const NX_MUTEX_LOCKER lock(&mutex);
                        runningRequestIds.erase(id);
                    }

                    executeRequests();
                });

            request(callback);
        };

    {
        const NX_MUTEX_LOCKER lock(&mutex);
        requests.push_back(std::make_pair(id, requestHolder));
        idToRequestItMapping.insert(std::make_pair(id, std::prev(requests.end())));
    }

    return id;
}

bool RequestQueue::Private::removeRequestUnsafe(const QUuid& id)
{
    const auto it = idToRequestItMapping.find(id);
    if (it == idToRequestItMapping.cend())
        return false;

    requests.erase(it->second);
    idToRequestItMapping.erase(it);

    return true;
}

void RequestQueue::Private::executeRequests()
{
    while (true)
    {
        Private::RequestHolder request;
        {
            const NX_MUTEX_LOCKER lock(&mutex);
            if (requests.empty() || runningRequestIds.size() >= maxRunningCount)
                return;

            const auto id = requests.front().first;
            request = requests.front().second;
            removeRequestUnsafe(id);
            runningRequestIds.insert(id);
        }

        request();
    }
}

void RequestQueue::Private::clear()
{
    const NX_MUTEX_LOCKER lock(&mutex);
    runningRequestIds.clear();
    requests.clear();
    idToRequestItMapping.clear();
}

//-------------------------------------------------------------------------------------------------

RequestQueue::RequestQueue(int maxRunningRequestsCount):
    d(new Private{this, maxRunningRequestsCount})
{
}

RequestQueue::~RequestQueue()
{
}

QUuid RequestQueue::addRequest(const Request& request)
{
    const auto result = d->addRequest(request);
    d->executeRequests();
    return result;
}

bool RequestQueue::removeRequest(const QUuid& id)
{
    const NX_MUTEX_LOCKER lock(&d->mutex);
    return d->removeRequestUnsafe(id);
}

void RequestQueue::clear()
{
    d->clear();
}

} // namespace nx::vms::client::core
