// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/qt_containers_reflect.h>

#include "common.h"

namespace nx::vms::api::rules {

struct AggregatedInfo
{
    /**
    * Total count of aggregated events. We are storing only limited subset of the original events
    * to the database, so we need it to restore the original count.
    */
    int total = 0;

    /**
    * Details for each event which happened in the beginning. Only limited subset of all events is
    * stored.
    */
    std::vector<PropertyMap> firstEventsData;

    /**
     * Details for each event which happened in the end. If total count of the events was less than
     * the limit, this list will be empty.
     */
    std::vector<PropertyMap> lastEventsData;
};
#define AggregatedInfo_Fields \
    (total)(firstEventsData)(lastEventsData)
NX_REFLECTION_INSTRUMENT(AggregatedInfo, AggregatedInfo_Fields)
QN_FUSION_DECLARE_FUNCTIONS(AggregatedInfo, (json), NX_VMS_API)

} // namespace nx::vms::api::rules
