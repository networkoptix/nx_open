// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/mobile/system_context_aware.h>

namespace nx::vms::client::mobile {

/**
 * Between versions 6.0.0 and 6.1.0 inclusive, servers don't automatically send to the Mobile Client
 * (in FullInfoData via the transaction message bus) all user groups the current user recursively
 * inherits from. It leads to incorrect client-side access rights computation. This workaround
 * explicitly requests all user groups via an API call immediately after connection.
 */
class UserRightsWorkaround:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    static void install(SystemContext* systemContext);

protected:
    explicit UserRightsWorkaround(SystemContext* systemContext, QObject* parent = nullptr);

private:
    bool isUserGroupLoaded(const nx::Uuid& groupId) const;
};

} // namespace nx::vms::client::mobile
