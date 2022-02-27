// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <future>

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
    return future.get();
}

} // namespace ec2::detail

