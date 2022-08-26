// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include "abstract_server_connection.h"

namespace nx::network::stun {

struct MessageContext
{
    std::shared_ptr<AbstractServerConnection> connection;
    stun::Message message;

    /**
     * Application-specific message attributes.
     */
    nx::utils::stree::StringAttrDict attrs;
};

/**
 * STUN message handler interface.
 */
class AbstractMessageHandler
{
public:
    virtual ~AbstractMessageHandler() = default;

    /**
     * Process the message.
     * The implementation is responsible for sending a reply through the ctx.connection.
     */
    virtual void serve(MessageContext ctx) = 0;
};

} // namespace nx::network::stun
