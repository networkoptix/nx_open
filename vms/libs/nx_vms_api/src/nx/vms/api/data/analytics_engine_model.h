// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::api {

struct NX_VMS_API AnalyticsEngineModel
{
    QnUuid id;
    QString name;
    QnUuid integrationId;

    QnUuid getId() const { return id; }

    AnalyticsEngineModel() = default;
    explicit AnalyticsEngineModel(AnalyticsEngineData data);

    using DbReadTypes = std::tuple<AnalyticsEngineData>;
    using DbListTypes = std::tuple<AnalyticsEngineDataList>;
    using DbUpdateTypes = std::tuple<AnalyticsEngineData>;

    DbUpdateTypes toDbTypes() &&;
    static std::vector<AnalyticsEngineModel> fromDbTypes(DbListTypes data);
    static AnalyticsEngineModel fromDb(AnalyticsEngineData data);
};
#define nx_vms_api_AnalyticsEngineModel_Fields (id)(integrationId)(name)
QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEngineModel, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(AnalyticsEngineModel, nx_vms_api_AnalyticsEngineModel_Fields);

} // namespace nx::vms::api
