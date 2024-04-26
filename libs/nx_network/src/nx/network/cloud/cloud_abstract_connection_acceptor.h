// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/abstract_stream_socket_acceptor.h>
#include <nx/network/http/http_status.h>
#include <nx/utils/url.h>

namespace nx::network::cloud {

/**
 * Error occurred while listening or accepting a cloud connection. E.g., error when connecting to a
 * relay server.
 */
struct NX_NETWORK_API AcceptorError
{
    nx::utils::Url remoteAddress;
    SystemError::ErrorCode sysErrorCode = SystemError::noError;
    std::optional<http::StatusCode::Value> httpStatusCode;

    bool operator==(const AcceptorError& other) const;

    std::string toString() const;
};

class AbstractConnectionAcceptor:
    public AbstractStreamSocketAcceptor
{
public:
    using ErrorHandler = nx::utils::MoveOnlyFunc<void(AcceptorError)>;

    /**
     * @return Ready-to-use connection from internal listen queue.
     *   nullptr if no connections available.
     */
    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() = 0;

    /**
     * Used to report errors from the specific acceptor implementation up
     * Up to the server socket.
     */
    virtual void setAcceptErrorHandler(ErrorHandler) = 0;
};

} // namespace nx::network::cloud
