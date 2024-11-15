// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "field.h"

namespace nx::vms::api::rules {

struct NX_VMS_API EventFilter
{
    /**%apidoc[readonly] Unique identifier for the event filter. */
    nx::Uuid id;

    /**%apidoc String description of this event filter type.
     * %example motion
     */
    QString type;

    /**%apidoc[opt] String Field Name to Field Description map.
     * Fields are additional parameters that can be passed to events. For example, you may have an
     * event that detects motion on a camera. One possible field you could set for this event would
     * be "device" which would allow you to specify the camera quality of the video that is
     * recorded. Fields are specific to an action.
     */
    std::map<QString, Field> fields;
};

#define nx_vms_api_rules_EventFilter_Fields (id)(type)(fields)
NX_VMS_API_DECLARE_STRUCT_EX(EventFilter, (json)(ubjson))
NX_REFLECTION_INSTRUMENT(EventFilter, nx_vms_api_rules_EventFilter_Fields)

} // namespace nx::vms::api::rules
