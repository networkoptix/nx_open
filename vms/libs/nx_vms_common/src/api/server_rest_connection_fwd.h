// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include <nx/utils/move_only_func.h>
#include <nx/vms/api/data/json_rpc.h>

namespace nx::network::rest { struct JsonResult; }

namespace rest {

class Status;

using Handle = int; // TODO: Make dependend on nx::network::http::ClientPool handle type.

struct EmptyResponseType;

template<typename ResultType>
using Callback = nx::MoveOnlyFunc<void(Status status, Handle requestId, ResultType result)>;

using JsonRpcBatchResultCallback = Callback<std::vector<nx::vms::api::JsonRpcResponse>>;

class ServerConnection;
using ServerConnectionPtr = std::shared_ptr<ServerConnection>;
using ServerConnectionWeakPtr = std::weak_ptr<ServerConnection>;

static constexpr std::chrono::seconds kDefaultVmsApiTimeout = std::chrono::seconds(30);

// Should be less than rest::kDefaultVmsApiTimeout which is used by client by default.
static constexpr std::chrono::seconds kDefaultDelegateTimeout = std::chrono::seconds(15);

// Legacy callback types. Do not use with modern API.
using JsonResultCallback = Callback<nx::network::rest::JsonResult>;
using EmptyResultCallback = Callback<EmptyResponseType>;

}; // namespace rest
