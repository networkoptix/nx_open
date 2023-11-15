// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/network/http/http_types.h>
#include <nx/utils/move_only_func.h>

#include "result_code.h"
#include "system_data.h"

namespace nx::cloud::db::api {

class BatchUserProcessingManager
{
public:
    virtual ~BatchUserProcessingManager() = default;

    virtual void createUpdateBatch(
        const api::CreateBatchRequest& request,
        std::function<void(api::ResultCode, api::CreateBatchResponse)> completionHandler) = 0;

    virtual void getBatchState(
        const std::string & batchId,
        std::function<void(api::ResultCode, api::BatchState)> completionHandler) = 0;

    virtual void getBatchErrorInfo(
        const std::string& batchId,
        std::function<void(api::ResultCode, api::BatchErrorInfo)> completionHandler) = 0;
};

}
