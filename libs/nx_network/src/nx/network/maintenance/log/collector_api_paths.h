// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::network::maintenance::log {

static constexpr char kLogCollectorPathPrefix[] = "/log-collector/v1";

static constexpr char kLogSessionFragmentsPath[] = "/log-session/{sessionId}/log/fragment";
static constexpr char kLogSessionIdParam[] = "sessionId";

} // namespace nx::network::maintenance::log
