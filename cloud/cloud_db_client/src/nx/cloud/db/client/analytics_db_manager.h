// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/cloud/db/api/analytics_db_manager.h>

#include "async_http_requests_executor.h"

namespace nx::cloud::db::client {

class AnalyticsDbManager:
    public api::AnalyticsDbManager
{
public:

    AnalyticsDbManager(
        ApiRequestsExecutor* requestsExecutor);

    virtual void saveTracks(
        bool autoVectorize,
        const api::SaveTracksData& data,
        nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void getTracks(
        const api::GetTracksParamsData& filter,
        nx::MoveOnlyFunc<void(api::ResultCode, const api::ReadTracksData& data)> completionHandler) override;

    virtual void getTrackPeriods(
        const api::GetTracksParamsData& filter,
        nx::MoveOnlyFunc<void(api::ResultCode, const api::TimePeriodList& data)> completionHandler) override;

    virtual void getBestShotImage(
        const std::string& orgId,
        const std::string& siteId,
        const nx::Uuid& trackId,
        nx::MoveOnlyFunc<void(api::ResultCode, const api::TrackImageData& image)> completionHandler) override;

    virtual void getTitleImage(
        const std::string& orgId,
        const std::string& siteId,
        const nx::Uuid& trackId,
        nx::MoveOnlyFunc<void(api::ResultCode, const api::TrackImageData& image)> completionHandler) override;

    virtual void deleteTracks(
        const std::string& orgId,
        const std::string& siteId,
        std::chrono::milliseconds time,
        nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void saveTaxonomy(
        const std::string& orgId,
        const std::string& siteId,
        const api::TaxonomyData& data,
        nx::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    virtual void getTaxonomy(
        const std::string& orgId,
        const std::string& siteId,
        nx::MoveOnlyFunc<void(api::ResultCode, api::TaxonomyData)> completionHandler) override;

    static void setCustomUrl(const QString& url);

private:
    ApiRequestsExecutor* requestExecutor();
private:
    std::unique_ptr<ApiRequestsExecutor> m_customRequestExecutor;
    ApiRequestsExecutor* m_requestsExecutor = nullptr;
    static QString m_customUrl;
};

} // namespace nx::cloud::db::api
