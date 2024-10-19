// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "push_notification_structures.h"

namespace nx::network::http { class Credentials; }
namespace nx::network::http { class AsyncClient; }

namespace nx::vms::client::mobile::details {

using HttpClientPtr = QSharedPointer<nx::network::http::AsyncClient>;
using HttpClientWeakPtr = QWeakPointer<nx::network::http::AsyncClient>;

class PushApiHelper
{
public:
    using Callback = std::function<void (bool /*success*/)>;

    static HttpClientPtr createClient();

    static void subscribe(
        const nx::network::http::Credentials& credentials,
        const TokenData& tokenData,
        const SystemSet& systems,
        const HttpClientPtr& client,
        Callback callback);

    static void unsubscribe(
        const nx::network::http::Credentials& credentials,
        const TokenData& tokenData,
        const HttpClientPtr& client,
        Callback callback = Callback());
};

} // namespace nx::vms::client::mobile
