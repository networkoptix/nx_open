// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API EngineModel
{
    nx::Uuid id;
    QString name;
    nx::Uuid integrationId;
};
#define nx_vms_api_analytics_EngineModel_Fields (id)(name)(integrationId)
QN_FUSION_DECLARE_FUNCTIONS(EngineModel, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(EngineModel, nx_vms_api_analytics_EngineModel_Fields);

} // namespace nx::vms::api::analytics
