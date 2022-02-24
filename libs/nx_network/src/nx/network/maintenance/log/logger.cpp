// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logger.h"

#include <nx/fusion/model_functions.h>

namespace nx::network::maintenance::log {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Filter, (json), Filter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Logger, (json), Logger_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Loggers, (json), Loggers_Fields)

} // namespace nx::network::maintenance::log
