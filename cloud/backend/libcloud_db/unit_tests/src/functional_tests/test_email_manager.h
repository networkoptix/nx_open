#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/cloud/cdb/managers/email_manager.h>

namespace nx::cdb {

class TestEmailManager:
    public AbstractEmailManager
{
public:
    TestEmailManager(
        nx::utils::MoveOnlyFunc<void(const AbstractNotification&)> _delegate);

    virtual void sendAsync(
        const AbstractNotification& notification,
        std::function<void(bool)> completionHandler) override;

private:
    nx::utils::MoveOnlyFunc<void(const AbstractNotification&)> m_delegate;
};

} // namespace nx::cdb
