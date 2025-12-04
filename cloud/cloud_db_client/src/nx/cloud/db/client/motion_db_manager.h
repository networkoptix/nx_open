// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/motion_db_manager.h>

#include "async_http_requests_executor.h"

namespace nx::cloud::db::client {

class MotionDbManager:
    public api::MotionDbManager
{
public:

    MotionDbManager(
        ApiRequestsExecutor* requestsExecutor);

    virtual void saveMotion(
        const std::vector<api::Motion>& data,
        nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void getMotionPeriods(
        const api::GetMotionParamsData& filter,
        nx::MoveOnlyFunc<void(api::ResultCode, const api::TimePeriodList& data)> completionHandler) override;

    static void setCustomUrl(const QString& url);

private:
    ApiRequestsExecutor* requestExecutor();
private:
    std::unique_ptr<ApiRequestsExecutor> m_customRequestExecutor;
    ApiRequestsExecutor* m_requestsExecutor = nullptr;
    static QString m_customUrl;
};

} // namespace nx::cloud::db::client
