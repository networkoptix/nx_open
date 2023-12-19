// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/batch_user_processing_manager.h"

namespace nx::cloud::db::client {

class BatchUserProcessingManager:
    public api::BatchUserProcessingManager
{
public:
    BatchUserProcessingManager(ApiRequestsExecutor* requestsExecutor);

    virtual void createUpdateBatch(
        const api::CreateBatchRequest& request,
        std::function<void(api::ResultCode, api::CreateBatchResponse)> completionHandler) override;

    virtual void getBatchState(
        const std::string & batchId,
        std::function<void(api::ResultCode, api::BatchState)> completionHandler) override;

     virtual void getBatchErrorInfo(
        const std::string& batchId,
        std::function<void(api::ResultCode, api::BatchErrorInfo)> completionHandler) override;

private:
    ApiRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::client
