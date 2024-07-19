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
QN_FUSION_DECLARE_FUNCTIONS(AvailableAnalyticsActionsOfEngine, (json), NX_VMS_COMMON_API)

// TODO: Consider using the one from NX_VMS_API. However, the new one has `actionIds` instead...
struct NX_VMS_COMMON_API LegacyAvailableAnalyticsActions
{
    QList<AvailableAnalyticsActionsOfEngine> actions;
};
#define LegacyAvailableAnalyticsActions_Fields (actions)
QN_FUSION_DECLARE_FUNCTIONS(LegacyAvailableAnalyticsActions, (json), NX_VMS_COMMON_API)
