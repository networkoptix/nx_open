// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <ranges>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QHash>

#include <nx/reflect/json.h>
#include <nx/utils/log/format.h>
#include <nx/vms/client/mobile/push_notification/details/abstract_secure_storage.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>

namespace nx::vms::client::mobile {

std::ostream& operator<<(std::ostream& stream, const PushNotification& notification)
{
    return stream << nx::reflect::json::serialize(notification);
}

namespace test {

class MockSecureStorage: public AbstractSecureStorage
{
public:
    virtual std::optional<std::string> load(const std::string& key) const
    {
        return m_items[key];
    }

    virtual void save(const std::string& key, const std::string& value)
    {
        m_items[key] = value;
    }

    virtual std::optional<std::vector<std::byte>> loadFile(const std::string& key) const
    {
        if (!m_files.contains(key))
            return std::nullopt;

        return m_files[key];
    }

    virtual void saveFile(const std::string& key, const std::vector<std::byte>& data)
    {
        m_files[key] = data;
    }

    virtual void removeFile(const std::string& key)
    {
        m_files.remove(key);
    }

private:
    QHash<std::string, std::string> m_items;
    QHash<std::string, std::vector<std::byte>> m_files;
};

class PushNotificationStorageTest: public testing::Test
{
public:
    static constexpr auto kUser = "user";

    virtual void SetUp() override
    {
        auto currentTime = [time = 0]() mutable { return std::chrono::milliseconds{++time}; };
        m_pushNotificationStorage = std::make_unique<PushNotificationStorage>(
            std::make_shared<MockSecureStorage>(), currentTime);
    }

    QHash<std::string, PushNotification> addNotifications(const std::string& user, int count)
    {
        QHash<std::string, PushNotification> result;

        for (int i = 0; i < count; ++i, ++m_notificationCount)
        {
            PushNotification notification = {
                .title = nx::format("Title %1", m_notificationCount).toStdString(),
                .description = nx::format("Description %1", m_notificationCount).toStdString(),
                .url = nx::format("Url %1", m_notificationCount).toStdString(),
                .cloudSystemId = nx::format("CloudSystemId %1", m_notificationCount).toStdString(),
                .imageUrl = nx::format("ImageUrl %1", m_notificationCount).toStdString()
            };

            notification.id = m_pushNotificationStorage->addUserNotification(
                user,
                notification.title,
                notification.description,
                notification.url,
                notification.cloudSystemId,
                notification.imageUrl);

            result[notification.id] = std::move(notification);
        }

        const auto notifications = userNotifications(user);
        for (const PushNotification& notification: notifications)
            result[notification.id].time = notification.time;

        return result;
    }

    std::vector<PushNotification> userNotifications(const std::string& user)
    {
        return m_pushNotificationStorage->userNotifications(user);
    }

    std::vector<std::byte> makeImage(uint i) { return std::vector<std::byte>{i}; };

    PushNotificationStorage* pushNotificationStorage() const
    {
        return m_pushNotificationStorage.get();
    }

private:
    int m_notificationCount = 0;
    std::unique_ptr<PushNotificationStorage> m_pushNotificationStorage;
};

TEST_F(PushNotificationStorageTest, sorting)
{
    addNotifications(kUser, 3);
    const auto notifications = userNotifications(kUser);
    const bool isSorted = std::ranges::is_sorted(notifications,
        [](const PushNotification& left, const PushNotification& right)
        {
            return left.time > right.time;
        });

    EXPECT_TRUE(isSorted);
}

TEST_F(PushNotificationStorageTest, addUserNotification)
{
    constexpr auto kUser1 = "user1";
    constexpr auto kUser2 = "user2";

    const auto expectedNotifications1 = addNotifications(kUser1, 3);
    const auto expectedNotifications2 = addNotifications(kUser2, 3);

    for (const auto& [user, expected]: {
        std::pair{kUser1, expectedNotifications1},
        std::pair{kUser2, expectedNotifications2}})
    {
        for (const PushNotification& notification: userNotifications(user))
        {
            EXPECT_TRUE(notification.time > std::chrono::milliseconds{0});
            EXPECT_FALSE(notification.id.empty());
            EXPECT_FALSE(notification.isRead);
            EXPECT_EQ(notification, expected[notification.id]);
        }
    }
}

TEST_F(PushNotificationStorageTest, setIsRead)
{
    const auto notification = addNotifications(kUser, 1).values().first();

    pushNotificationStorage()->setIsRead(kUser, notification.id, true);
    EXPECT_TRUE(userNotifications(kUser).front().isRead);

    pushNotificationStorage()->setIsRead(kUser, notification.id, false);
    EXPECT_FALSE(userNotifications(kUser).front().isRead);
}

TEST_F(PushNotificationStorageTest, saveImage)
{
    pushNotificationStorage()->saveImage("1", makeImage(1));
    pushNotificationStorage()->saveImage("2", makeImage(2));

    EXPECT_EQ(pushNotificationStorage()->findImage("1"), makeImage(1));
    EXPECT_EQ(pushNotificationStorage()->findImage("2"), makeImage(2));
    EXPECT_FALSE(pushNotificationStorage()->findImage("3").has_value());
}

TEST_F(PushNotificationStorageTest, capacity)
{
    const PushNotification first = addNotifications(kUser, 1).values().first();
    pushNotificationStorage()->saveImage(first.imageUrl, makeImage(1));

    const auto expectedNotifications =
        addNotifications(kUser, PushNotificationStorage::kUserStorageCapacity);

    const auto notifications = userNotifications(kUser);
    EXPECT_EQ(notifications.size(), PushNotificationStorage::kUserStorageCapacity);

    const bool isFirstRemoved = std::ranges::none_of(notifications,
        [&first](const PushNotification& notification) { return notification.id == first.id; });

    EXPECT_TRUE(isFirstRemoved);
    EXPECT_FALSE(pushNotificationStorage()->findImage(first.imageUrl).has_value());

    for (PushNotification& notification: userNotifications(kUser))
        EXPECT_EQ(notification, expectedNotifications[notification.id]);
}

TEST_F(PushNotificationStorageTest, totalCapacity)
{
    auto user = [](int i) { return std::to_string(i); };

    for (int i = 0; i < PushNotificationStorage::kExpectedUserCount; ++i)
        addNotifications(user(i), PushNotificationStorage::kUserStorageCapacity);

    // When the total storage size reaches its maximum and a new notification is added, the other
    // storages are reduced evenly in size.
    addNotifications(kUser, PushNotificationStorage::kExpectedUserCount);

    for (int i = 0; i < PushNotificationStorage::kExpectedUserCount; ++i)
    {
        const auto notifications = userNotifications(user(i));
        EXPECT_EQ(notifications.size(), PushNotificationStorage::kUserStorageCapacity - 1);
    }
}

} // namespace test
} // namespace nx::vms::client::mobile
