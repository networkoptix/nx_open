// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/basic_pollable.h>

#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>

#include <nx/network/cloud/mediator/api/connection_speed.h>

namespace nx::network::cloud::speed_test {

using CompletionHandler =
    nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode,
        std::optional<nx::hpm::api::ConnectionSpeed>)>;

class AbstractSpeedTester:
    public aio::BasicPollable
{
public:
    struct Settings {
        nx::utils::Url url;
        int maxPingRequests = 0;
        int minBandwidthRequests = 5;
        std::chrono::milliseconds testDuration{0};
    };

    virtual ~AbstractSpeedTester() = default;

    virtual void start(CompletionHandler handler) = 0;
};

} // namespace nx::network::cloud::speed_test
