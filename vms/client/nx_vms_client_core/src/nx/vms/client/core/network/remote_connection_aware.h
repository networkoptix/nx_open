// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/socket_common.h>
#include <nx/vms/client/core/network/connection_info.h>
#include <nx/vms/client/core/network/remote_connection_fwd.h>

namespace ec2 {

class AbstractECConnection;
using AbstractECConnectionPtr = std::shared_ptr<AbstractECConnection>;

} // namespace ec2

namespace rest {

class ServerConnection;
using ServerConnectionPtr = std::shared_ptr<ServerConnection>;

} // namespace rest

namespace nx::vms::api {

struct UserGroupData;

} // namespace nx::vms::api

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API RemoteConnectionAwareMock: public ConnectionInfo
{
public:
    virtual ~RemoteConnectionAwareMock() {}
    virtual bool saveUser(const QnUserResourcePtr&) { return true; }
    virtual bool saveGroup(const nx::vms::api::UserGroupData&) { return true; }
};

class NX_VMS_CLIENT_CORE_API RemoteConnectionAware
{
public:
    RemoteConnectionPtr connection() const;

    std::shared_ptr<RemoteSession> session() const;

    ec2::AbstractECConnectionPtr messageBusConnection() const;

    /** Address of the server we are currently connected to. */
    nx::network::SocketAddress connectionAddress() const;

    /** Credentials we are using to authorize the connection. */
    nx::network::http::Credentials connectionCredentials() const;

    /** API interface of the currently connected server. */
    rest::ServerConnectionPtr connectedServerApi() const;

    /** Sets the mock implementation. */
    void setMockImplementation(std::unique_ptr<RemoteConnectionAwareMock> implementation);

    /** Returns mock implementation currently set. */
    RemoteConnectionAwareMock* mockImplementation() const;

    nx::Uuid serverId() const;
    QnMediaServerResourcePtr currentServer() const;

private:
    std::unique_ptr<RemoteConnectionAwareMock> m_mockImplementation;
};

} // namespace nx::vms::client::core
