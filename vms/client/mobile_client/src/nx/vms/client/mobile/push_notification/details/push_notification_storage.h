// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nx/reflect/instrument.h>

#include "secure_storage.h"

namespace nx::vms::client::mobile {

struct PushNotification
{
    std::string id;
    std::string title;
    std::string description;
    std::string url;
    std::string cloudSystemId;
    std::string imageId;
    std::chrono::milliseconds time;
    bool isRead = false;
};
NX_REFLECTION_INSTRUMENT(PushNotification,
    (id)(time)(title)(description)(url)(cloudSystemId)(imageId))

class PushNotificationStorage
{
public:
    PushNotificationStorage(std::shared_ptr<SecureStorage> storage);
    ~PushNotificationStorage();

    std::vector<PushNotification> userNotifications(const std::string& user);
    std::optional<std::vector<std::byte>> loadImage(const std::string& id);
    void markAsRead(const std::string& user, const std::string& id);
    std::string addUserNotification(
        const std::string& user,
        const std::string& title,
        const std::string& description,
        const std::string& url,
        const std::string& cloudSystemId,
        const std::string& imageId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // nx::vms::client::mobile
