// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_manager.h"

#include <nx/utils/thread/mutex.h>

namespace nx::vms::common {

struct LookupListManager::Private
{
    mutable nx::Mutex mutex;
    bool initialized = false;
    nx::vms::api::LookupListDataList data;
};

LookupListManager::LookupListManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

LookupListManager::~LookupListManager()
{
}

bool LookupListManager::isInitialized() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->initialized;
}

void LookupListManager::initialize(nx::vms::api::LookupListDataList initialData)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        d->data = std::move(initialData);
        d->initialized = true;
    }
    emit initialized();
}

void LookupListManager::deinitialize()
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    d->data.clear();
    d->initialized = false;
}

nx::vms::api::LookupListData LookupListManager::lookupList(const QnUuid& id) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    if (!d->initialized)
        return {};

    auto iter = std::find_if(d->data.cbegin(), d->data.cend(),
        [id](const auto& item) { return item.id == id; });

    if (iter != d->data.cend())
        return *iter;
    return {};
}

const nx::vms::api::LookupListDataList& LookupListManager::lookupLists() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->data;
}

void LookupListManager::addOrUpdate(nx::vms::api::LookupListData list)
{
    const auto id = list.id;

    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        if (!d->initialized)
            return;

        auto iter = std::find_if(d->data.begin(), d->data.end(),
            [id](const auto& item) { return item.id == id; });

        if (iter != d->data.end())
            *iter = std::move(list);
        else
            d->data.push_back(std::move(list));
    }

    emit addedOrUpdated(id);
}

QList<int> LookupListManager::filter(
    const nx::vms::api::LookupListData& list, const QString& filterText, int resultLimit) const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    QList<int> result;
    for (int i = 0; i < list.entries.size() && i < resultLimit; i++)
    {
        for (auto& [key, value]: list.entries[i])
        {
            if (value.contains(filterText))
                result.push_back(i);
        }
    }
    return result;
}

void LookupListManager::remove(const QnUuid& id)
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        if (!d->initialized)
            return;

        auto iter = std::find_if(d->data.begin(), d->data.end(),
            [id](const auto& item) { return item.id == id; });

        if (iter == d->data.end())
            return;

        d->data.erase(iter);
    }

    emit removed(id);
}

} // namespace nx::vms::common
