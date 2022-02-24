// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/address_resolver.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

namespace nx::network::cloud::tcp {

class NX_NETWORK_API AbstractEndpointVerificator:
    public aio::BasicPollable
{
public:
    enum class VerificationResult
    {
        passed,
        notPassed,
        ioError,
    };

    virtual void setTimeout(std::chrono::milliseconds timeout) = 0;

    /**
     * No interleaved verifications are allowed!
     */
    virtual void verifyHost(
        const SocketAddress& endpointToVerify,
        const AddressEntry& desiredHostAddress,
        nx::utils::MoveOnlyFunc<void(VerificationResult)> completionHandler) = 0;

    virtual SystemError::ErrorCode lastSystemErrorCode() const = 0;

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() = 0;
};

} // namespace nx::network::cloud::tcp
