// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_notification_storage.h"

#include <cctype>
#include <functional>
#include <vector>
#include <ranges>

#include <nx/reflect/json.h>

namespace nx::vms::client::mobile {

namespace {

constexpr auto kStorageKey = "push_notifications";
constexpr auto kStorageCapacity = 100;
constexpr auto kTotalStorageCapacity = 2 * kStorageCapacity;

using ItemRemovedCallback = std::function<void(const PushNotification&)>;

std::chrono::milliseconds now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

struct Storage
{
    std::vector<PushNotification> items; //< Sorted by time in descending order.

    int size() const { return items.size(); }
    bool empty() const { return items.empty(); }

    void add(PushNotification&& notification)
    {
        const auto insertIt = std::lower_bound(items.begin(), items.end(), notification,
            [](const PushNotification& left, const PushNotification& right)
            {
                return left.time > right.time;
            });

        items.insert(insertIt, std::move(notification));
    }

    PushNotification* find(const std::string& id)
    {
        auto it = std::find_if(items.begin(), items.end(),
            [id](const PushNotification& item) { return item.id == id; });

        return it != items.end() ? &*it : nullptr;
    }

    void maintainCapacity(ItemRemovedCallback onRemoved)
    {
        if (size() > kStorageCapacity)
        {
            std::for_each(items.begin() + kStorageCapacity, items.end(), onRemoved);
            items.resize(kStorageCapacity);
        }
    }

    void removeLast(ItemRemovedCallback onRemoved)
    {
        if (items.empty())
            return;

        onRemoved(items.back());
        items.pop_back();
    }
};
NX_REFLECTION_INSTRUMENT(Storage, (items));

struct Data
{
    std::unordered_map<std::string, Storage> storages;

    int size() const
    {
        int result = 0;
        for (const auto& [_, storage]: storages)
            result += storage.size();

        return result;
    }

    Storage* ensureUserStorage(const std::string& user)
    {
        return &storages[user];
    }

    std::vector<PushNotification> userNotifications(const std::string& user)
    {
        return ensureUserStorage(user)->items;
    }

    PushNotification* find(const std::string& user, const std::string& id)
    {
        return ensureUserStorage(user)->find(id);
    }

    void addUserNotification(
        const std::string& user,
        PushNotification&& notification,
        ItemRemovedCallback onRemoved)
    {
        Storage* storage = ensureUserStorage(user);
        storage->add(std::move(notification));
        storage->maintainCapacity(onRemoved);

        // Remove notifications one by one, making the other storages equal in size.
        int removeCount = size() - kTotalStorageCapacity;
        if (removeCount <= 0)
            return;

        using namespace std::ranges;

        // There is only one user in most cases.
        auto excludeCurrent = views::filter(
            [&user](const auto& keyValue) { return keyValue.first != user; });

        auto removeOne =
            [&, this]()
            {
                auto items = storages | excludeCurrent | views::values;

                const auto it = max_element(items,
                    [](const Storage& left, const Storage& right)
                    {
                        return left.size() < right.size();
                    });

                if (it == items.end() || (*it).empty())
                    return false;

                (*it).removeLast(onRemoved);
                return true;
            };

        while (removeCount-- > 0) //< Most likely, removeCount is 1.
        {
            if (!removeOne())
                return;
        }
    }
};
NX_REFLECTION_INSTRUMENT(Data, (storages));

} // namespace

struct PushNotificationStorage::Private
{
    std::shared_ptr<SecureStorage> storage;

    std::optional<Data> load()
    {
        const auto serializedData = storage->load(kStorageKey);

        if (!serializedData || serializedData->empty())
            return Data{};

        auto [data, result] = nx::reflect::json::deserialize<Data>(*serializedData);
        if (!result)
            return Data{};

        return data;
    }

    void save(const Data& data)
    {
        const std::string result = nx::reflect::json::serialize(data);
        storage->save(kStorageKey, result);
    }

    void removeImage(const PushNotification& notification)
    {
        if (!notification.imageUrl.empty())
            storage->removeFile(/*key*/ notification.imageUrl);
    };
};

PushNotificationStorage::PushNotificationStorage(std::shared_ptr<SecureStorage> storage):
    d(std::make_unique<Private>())
{
    d->storage = storage;
}

std::string PushNotificationStorage::addUserNotification(
    const std::string& user,
    const std::string& title,
    const std::string& description,
    const std::string& url,
    const std::string& cloudSystemId,
    const std::string& imageUrl)
{
    if (auto data = d->load())
    {
        auto onRemoved =
            [this](const PushNotification& notification) { d->removeImage(notification); };

        const auto time = now();
        const auto id = std::to_string(time.count());
        data->addUserNotification(user, PushNotification{
            .id = id,
            .title = title,
            .description = description,
            .url = url,
            .cloudSystemId = cloudSystemId,
            .imageUrl = imageUrl,
            .time = time,
        },
        onRemoved);

        d->save(*data);

        return id;
    }

    return {};
}

std::vector<PushNotification> PushNotificationStorage::userNotifications(const std::string& user)
{
    if (auto data = d->load())
        return data->userNotifications(user);

    return {};
}

void PushNotificationStorage::setIsRead(const std::string& user, const std::string& id, bool value)
{
    if (auto data = d->load())
    {
        if (PushNotification* notification = data->find(user, id))
            notification->isRead = value;

        d->save(*data);
    }
}

std::optional<std::vector<std::byte>> PushNotificationStorage::findImage(const std::string& url)
{
    return d->storage->loadFile(/*key*/ url);
}

void PushNotificationStorage::saveImage(
    const std::string& url,
    const std::vector<std::byte>& image)
{
    d->storage->saveFile(/*key*/ url, image);
}

PushNotificationStorage::~PushNotificationStorage()
{
}

} // namespace nx::vms::client::mobile
