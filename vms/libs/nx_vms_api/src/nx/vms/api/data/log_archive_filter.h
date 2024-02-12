// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/json/value_or_array.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(LogName,
    main = static_cast<int>(nx::log::LogName::main),
    http = static_cast<int>(nx::log::LogName::http),
    system = static_cast<int>(nx::log::LogName::system),
    update
);

struct NX_VMS_API LogArchiveFilter
{
    nx::Uuid id;

    /**%apidoc Indicates if the archive includes (and how many) rotated files. */
    std::optional<int> rotated;

    /**%apidoc:stringArray
     * Logs included in the archive.
     *     %value main
     *     %value http
     *     %value system
     *     %value update
     * %// NOTE: need to specify values explicitly since apidoc doesn't parse ValueOrArray
     */
    std::optional<json::ValueOrArray<LogName>> names;
};
#define LogArchiveFilter_Fields (id)(rotated)(names)
QN_FUSION_DECLARE_FUNCTIONS(LogArchiveFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(LogArchiveFilter, LogArchiveFilter_Fields)

} // namespace nx::vms::api
