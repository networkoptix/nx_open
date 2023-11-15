// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/type_traits.h>

#include "data_macros.h"

namespace nx::vms::api {

struct NX_VMS_API ScriptInfo
{
    /**%apidoc:string
     * Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or be "this" to
     * refer to the current Server.
     * %example this
     */
    QnUuid id;

    std::string name;
    std::vector<std::string> args;
    bool waitForFinished{false};
};
#define ScriptInfo_Fields (id)(name)(args)(waitForFinished)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(ScriptInfo, (json))
NX_REFLECTION_INSTRUMENT(ScriptInfo, ScriptInfo_Fields)

struct NX_VMS_API RunProcessResult
{
    std::optional<int> returnCode;
};
#define RunProcessResult_Fields (returnCode)
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(RunProcessResult, (json))
NX_REFLECTION_INSTRUMENT(RunProcessResult, RunProcessResult_Fields)

} // namespace nx::vms::api
