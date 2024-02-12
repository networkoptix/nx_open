// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API EngineModel
{
    nx::Uuid id;
    QString name;
    nx::Uuid integrationId;

    nx::Uuid getId() const { return id; }

    EngineModel() = default;
    explicit EngineModel(AnalyticsEngineData data);

    using DbReadTypes = std::tuple<AnalyticsEngineData>;
    using DbListTypes = std::tuple<AnalyticsEngineDataList>;
    using DbUpdateTypes = std::tuple<AnalyticsEngineData>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<EngineModel> fromDbTypes(DbListTypes data);
    static EngineModel fromDb(AnalyticsEngineData data);
};
#define nx_vms_api_analytics_EngineModel_Fields (id)(integrationId)(name)
QN_FUSION_DECLARE_FUNCTIONS(EngineModel, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(EngineModel, nx_vms_api_analytics_EngineModel_Fields);

} // namespace nx::vms::api::analytics
