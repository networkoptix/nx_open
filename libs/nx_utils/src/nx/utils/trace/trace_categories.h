// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#define PERFETTO_COMPONENT_EXPORT NX_UTILS_API

#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("rendering")
        .SetDescription("Events from the graphics subsystem"),
    perfetto::Category("resources")
        .SetDescription("System resources (CPU, RAM etc.) utilization statistics"),
    perfetto::Category("models")
        .SetDescription("Events from data models")
);
