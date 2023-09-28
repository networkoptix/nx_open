// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <memory>

#include <nx/utils/log/log.h>

#include "../ec_api_common.h"

namespace ec2::detail {

template <typename Initiator, typename... Results>
Result callSync(Initiator initiate, Results*... outResults)
{
    auto promise = std::make_shared<std::promise<Result>>();
    auto future = promise->get_future();
    initiate(
        [&, promise = std::move(promise)](
            int /*reqId*/, Result errorCode, const Results&... results)
        {
            ((*outResults = results), ...);
            promise->set_value(errorCode);
        });

    /* future_error should only happen in unit-tests, never in stand-alone server */
    try
    {
        return future.get();
    }
    catch(const std::future_error& e)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Thread pool was cleared before executing the handler %1: %2",
            typeid(initiate), e.what());
        return ErrorCode::asyncRaceError;
    }
}

} // namespace ec2::detail
