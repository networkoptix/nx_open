#pragma once

#include <gmock/gmock.h>

#include <libcloud_db/src/managers/email_manager.h>

class EmailManagerMocked:
    public nx::cdb::AbstractEmailManager
{
public:
    MOCK_METHOD1(
        sendAsyncMocked,
        void(const nx::cdb::AbstractNotification& notification));

    virtual void sendAsync(
        const nx::cdb::AbstractNotification& notification,
        std::function<void(bool)> completionHandler) override;
};
