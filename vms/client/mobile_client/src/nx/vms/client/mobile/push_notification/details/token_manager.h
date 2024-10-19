// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <string>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "push_notification_structures.h"

namespace nx::vms::client::mobile::details {

/**
 * Incapsulates device token management for push notification purposes.
 * Device token is an unique devce identifier used by push notification management system to
 * identify device and route notifications. We need to have device token and send it with each
 * subscribe/unsubscribe request to our cloud instance.
 */
class TokenManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    TokenManager(QObject* parent = nullptr);
    virtual ~TokenManager() override;

    using RequestCallback = std::function<void (const TokenData& data)>;
    void requestToken(
        RequestCallback callback,
        bool forceUpdate);

    using ResetCallback = std::function<void (bool success)>;
    void resetToken(ResetCallback callback);

    TokenProviderType providerType() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile::details
