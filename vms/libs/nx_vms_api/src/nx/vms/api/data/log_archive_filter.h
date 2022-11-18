// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api {

struct NX_VMS_API LogArchiveFilter
{
    QnUuid id;

    /**%apidoc Indicates if the archive includes (and how many) rotated files. */
    std::optional<int> rotated;

    /**%apidoc:stringArray Logs included in the archive. */
    std::optional<json::ValueOrArray<nx::log::LogName>> names;
};
#define LogArchiveFilter_Fields (id)(rotated)(names)
QN_FUSION_DECLARE_FUNCTIONS(LogArchiveFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LogArchiveFilter, LogArchiveFilter_Fields)

} // namespace nx::vms::api
