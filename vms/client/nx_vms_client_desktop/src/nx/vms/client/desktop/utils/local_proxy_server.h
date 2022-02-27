// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/socks5/server.h>
#include <nx/utils/singleton.h>

namespace nx::vms::client::desktop {

/**
 * Runs a SOCKS5 local (accepts only localhost connections) proxy server that allows to tunnel
 * a connection throught any vms servers to its resources.
 * Example: if a camera C belongs to a server S, then the connection is tunneled via S to C.
 * In orded to do so the login for SOCKS5 server should be the uuid string value of C,
 * and the password should be the SOCKS5 server generated password string.
 */
class LocalProxyServer: public QObject, public Singleton<LocalProxyServer>
{
    Q_OBJECT

public:
    LocalProxyServer();
    virtual ~LocalProxyServer() override;
    nx::network::SocketAddress address() const;

    std::string password() const;

private:
    std::unique_ptr<nx::network::socks5::Server> m_server;
    std::string m_password;
};

} // namespace nx::vms::client::desktop
