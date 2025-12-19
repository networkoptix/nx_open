// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

namespace nx::vms::client::mobile {

class AbstractSecureStorage;

struct PushNotification
{
    std::string id;
    std::string title;
    std::string description;
    std::string url;
    std::string cloudSystemId;
    std::string imageUrl;
    std::chrono::milliseconds time;
    bool isRead = false;

    bool operator==(const PushNotification&) const = default;
};
NX_REFLECTION_INSTRUMENT(PushNotification,
    (id)(time)(title)(description)(url)(cloudSystemId)(imageUrl)(isRead))

class PushNotificationStorage
{
public:
    // PushNotificationStorage has a separate storage for each user with its own capacity.
    // Additionally, a total capacity is maintained by removing the earliest items from other
    // storages one by one making them equal in size.
    static constexpr int kUserStorageCapacity = 100;
    static constexpr int kExpectedUserCount = 2;
    static constexpr int kTotalStorageCapacity = kExpectedUserCount * kUserStorageCapacity;

public:
    PushNotificationStorage(
        std::shared_ptr<AbstractSecureStorage> storage,
        std::function<std::chrono::milliseconds()> currentTime = {});

    ~PushNotificationStorage();

    std::vector<PushNotification> userNotifications(const std::string& user);
    std::optional<std::vector<std::byte>> findImage(const std::string& url);
    void saveImage(const std::string& url, const std::vector<std::byte>& image);
    void setIsRead(const std::string& user, const std::string& id, bool value);
    std::string addUserNotification(
        const std::string& user,
        const std::string& title,
        const std::string& description,
        const std::string& url,
        const std::string& cloudSystemId,
        const std::string& imageUrl);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // nx::vms::client::mobile
