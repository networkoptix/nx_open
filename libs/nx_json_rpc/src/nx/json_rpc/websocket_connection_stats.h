// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

namespace nx::json_rpc {

struct NX_JSON_RPC_API WebSocketConnectionMessages
{
    size_t requests = 0;
    size_t responses = 0;
    size_t notifications = 0;
};

struct NX_JSON_RPC_API WebSocketConnectionStats
{
    WebSocketConnectionMessages in;
    WebSocketConnectionMessages out;

    size_t totalInB = 0;
    size_t totalOutB = 0;

    std::vector<QString> guards;
};

} // namespace nx::json_rpc
