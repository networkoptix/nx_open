// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "request_queue.h"

#include <list>
#include <mutex>
#include <unordered_map>

#include <nx/utils/guarded_callback.h>

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

using LockGuard = std::lock_guard<std::mutex>;

struct RequestQueue::Private
{
    using RequestHolder = std::function<void ()>;
    using RequestsContainer = std::list<std::pair<QUuid, RequestHolder>>;
    using RequestIterator = RequestsContainer::iterator;
    using IdToItContainer = std::unordered_map<QUuid, RequestIterator>;

    RequestQueue* const q;
    int maxRunningCount = 1;

    int runningCount = 0;
    std::mutex mutex;
    RequestsContainer requests;
    IdToItContainer idsMapping;

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
        [this, request]()
        {
            const auto callback = nx::utils::guarded(q,
                [this]()
                {
                    {
                        const LockGuard guard(mutex);
                        --runningCount;
                    }

                    executeRequests();
                });

            request(callback);
        };

    {
        const LockGuard guard(mutex);
        requests.push_back(std::make_pair(id, requestHolder));
        idsMapping.insert(std::make_pair(id, std::prev(requests.end())));
    }

    return id;
}

bool RequestQueue::Private::removeRequestUnsafe(const QUuid& id)
{
    const auto it = idsMapping.find(id);
    if (it == idsMapping.cend())
        return false;

    requests.erase(it->second);
    idsMapping.erase(it);

    return true;
}

void RequestQueue::Private::executeRequests()
{
    while (true)
    {
        Private::RequestHolder request;
        {
            const LockGuard guard(mutex);
            if (requests.empty() || runningCount >= maxRunningCount)
                return;

            request = requests.front().second;
            removeRequestUnsafe(requests.front().first);
            ++runningCount;
        }

        request();
    }
}

void RequestQueue::Private::clear()
{
    const LockGuard guard(mutex);
    runningCount = 0;
    requests.clear();
    idsMapping.clear();
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
    const LockGuard guard(d->mutex);
    return d->removeRequestUnsafe(id);
}

void RequestQueue::clear()
{
    d->clear();
}

} // namespace nx::vms::client::core
