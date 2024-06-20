// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <analytics/db/analytics_db_types.h>
#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API AvailableAnalyticsActionsOfEngine
{
    /**%apidoc Id of the Analytics Engine which offers the Actions. */
    nx::Uuid engineId;

    QStringList actionIds;
};
#define AvailableAnalyticsActionsOfEngine_Fields (engineId)(actionIds)

struct NX_VMS_COMMON_API AvailableAnalyticsActions
{
    QList<AvailableAnalyticsActionsOfEngine> actions;
};
#define AvailableAnalyticsActions_Fields (actions)

QN_FUSION_DECLARE_FUNCTIONS(AvailableAnalyticsActionsOfEngine, (json), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(AvailableAnalyticsActions, (json), NX_VMS_COMMON_API)
