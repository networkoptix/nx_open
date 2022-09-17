// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>
#include <nx/vms/common/system_context_aware.h>

class QnClientMessageProcessor;

namespace rest {

class ServerConnection;
using ServerConnectionPtr = std::shared_ptr<ServerConnection>;

} // namespace rest

namespace nx::vms::client::core {

class RemoteSession;
class ServerTimeWatcher;
class SystemContext;

class NX_VMS_CLIENT_CORE_API SystemContextAware: public nx::vms::common::SystemContextAware
{
public:
    using nx::vms::common::SystemContextAware::SystemContextAware;

    SystemContext* systemContext() const;

    QnClientMessageProcessor* clientMessageProcessor() const;

    /**
     * Id of the server which was used to establish the Remote Session (if it is present).
     */
    QnUuid currentServerId() const;

    /**
     * The server which was used to establish the Remote Session (if it is present).
     */
    QnMediaServerResourcePtr currentServer() const;

    RemoteConnectionPtr connection() const;

    /** API interface of the currently connected server. */
    rest::ServerConnectionPtr connectedServerApi() const;

    ServerTimeWatcher* serverTimeWatcher() const;
};

} // namespace nx::vms::client::desktop
