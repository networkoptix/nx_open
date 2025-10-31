// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

#include "analytics_db_data.h"
#include "result_code.h"

namespace nx::cloud::db::api {

/**
 * Contains actions available on analyticsDb.
 */
class AnalyticsDbManager
{
public:
    virtual ~AnalyticsDbManager() = default;

    /**
     * Update organization data.
     */
    virtual void saveTracks(
        bool autoVectorize,
        const SaveTracksData& data,
        nx::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void getTracks(
        const GetTracksParamsData& filter,
        nx::MoveOnlyFunc<void(ResultCode, const ReadTracksData& data)> completionHandler) = 0;

    virtual void getTrackPeriods(
        const api::GetTracksParamsData& filter,
        nx::MoveOnlyFunc<void(api::ResultCode, const api::TimePeriodList& data)> completionHandler) = 0;

    virtual void getBestShotImage(
        const std::string& orgId,
        const std::string& siteId,
        const nx::Uuid& trackId,
        nx::MoveOnlyFunc<void(ResultCode, const TrackImageData& image)> completionHandler) = 0;

    virtual void getTitleImage(
        const std::string& orgId,
        const std::string& siteId,
        const nx::Uuid& trackId,
        nx::MoveOnlyFunc<void(ResultCode, const TrackImageData& image)> completionHandler) = 0;

    virtual void deleteTracks(
        const std::string& orgId,
        const std::string& siteId,
        std::chrono::milliseconds time,
        nx::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void saveTaxonomy(
        const std::string& orgId,
        const std::string& siteId,
        const api::TaxonomyData& data,
        nx::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void getTaxonomy(
        const std::string& orgId,
        const std::string& siteId,
        nx::MoveOnlyFunc<void(ResultCode, TaxonomyData)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
