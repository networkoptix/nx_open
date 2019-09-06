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
    virtual ~AbstractSpeedTester() = default;

    virtual void start(const nx::utils::Url& speedTestUrl, CompletionHandler handler) = 0;
};

} // namespace nx::network::cloud::speed_test
