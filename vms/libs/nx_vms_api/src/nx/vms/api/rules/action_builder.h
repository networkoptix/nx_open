// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "field.h"

namespace nx::vms::api::rules {

struct NX_VMS_API ActionBuilder
{
    /**%apidoc[readonly] Unique identifier for the action builder. */
    nx::Uuid id;

    /**%apidoc
     * String description of this action filter type.
     * %example deviceRecording
     */
    QString type;

    /**%apidoc[opt]
     * String Field Name to Field Description map. Fields are additional parameters that can be
     * passed to actions. For example you may have an event that starts a recording. One possible
     * field you could set would be "streamQuality" which would allow you to set the quality of the
     * video that is recorded. Fields are specific to an action.
     */
    std::map<QString, Field> fields;
};

#define nx_vms_api_rules_ActionBuilder_Fields (id)(type)(fields)
NX_VMS_API_DECLARE_STRUCT_EX(ActionBuilder, (json)(ubjson))
NX_REFLECTION_INSTRUMENT(ActionBuilder, nx_vms_api_rules_ActionBuilder_Fields)

} // namespace nx::vms::api::rules
