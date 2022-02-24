// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <nx/vms/event//event_fwd.h>

namespace nx::network::http { class AsyncHttpClientPtr; }
namespace nx::network::rest { struct JsonResult; }
namespace nx::network::rest { class UbjsonResult; }

namespace rest {

#ifdef __arm__
    // Some devices lack kernel support for atomic int64.
    typedef int Handle;
#else
    typedef qint64 Handle;
#endif

template<typename ResultType>
using Callback = std::function<void(bool success, Handle requestId, ResultType result)>;

class ServerConnection;
using ServerConnectionPtr = std::shared_ptr<ServerConnection>;

using JsonResultCallback = Callback<nx::network::rest::JsonResult>;
using UbJsonResultCallback = Callback<nx::network::rest::UbjsonResult>;

}; // namespace rest
