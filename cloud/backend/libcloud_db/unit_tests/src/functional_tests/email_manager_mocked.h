#pragma once

#include <gmock/gmock.h>

#include <nx/utils/move_only_func.h>

#include <nx/cloud/cdb/managers/email_manager.h>

class TestEmailManager:
    public nx::cloud::db::AbstractEmailManager
{
public:
    typedef nx::utils::MoveOnlyFunc<void(const nx::cloud::db::AbstractNotification&)>
        OnNotificationHandler;

    virtual void sendAsync(
        const nx::cloud::db::AbstractNotification& notification,
        std::function<void(bool)> completionHandler) override;

    void setOnReceivedNotification(OnNotificationHandler handler);

private:
    OnNotificationHandler m_onNotificationHandler;
};

class EmailManagerMocked:
    public TestEmailManager
{
    typedef TestEmailManager BaseType;

public:
    MOCK_METHOD1(
        sendAsyncMocked,
        void(const nx::cloud::db::AbstractNotification& notification));

    virtual void sendAsync(
        const nx::cloud::db::AbstractNotification& notification,
        std::function<void(bool)> completionHandler) override;
};
