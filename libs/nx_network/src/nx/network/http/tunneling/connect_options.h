// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/url_query.h>

namespace nx::network::http::tunneling {

struct NX_NETWORK_API ConnectOptions
{
    static constexpr char kRunConnectionTest[] = "run-connection-test";
    static constexpr char kPeerPriority[] = "peer-priority";

    ConnectOptions() = default;
    ConnectOptions(std::initializer_list<std::pair<std::string, std::string>> list)
    {
        for (auto& [k, v]: list)
            options.emplace(std::move(k), std::move(v));
    }

    template<typename... Args>
	auto emplace(Args&&... args) { return options.emplace(std::forward<decltype(args)>(args)...); }

    nx::UrlQuery toUrlQuery() const;

    std::map<std::string, std::string> options;
};

} // namespace nx::network::http::tunneling
