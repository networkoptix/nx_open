// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "statistics.h"

#include <nx/fusion/model_functions.h>

namespace nx::hpm::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CloudConnectStatistics, (json), CloudConnectStatistics_Fields)

//-------------------------------------------------------------------------------------------------

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Country, (json), Country_mediator_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Continent, (json), Continent_mediator_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Location, (json), Location_mediator_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ListeningPeerStatistics, (json), ListeningPeerStatistics_mediator_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Statistics, (json), Statistics_mediator_Fields)

} // namespace nx::hpm::api
