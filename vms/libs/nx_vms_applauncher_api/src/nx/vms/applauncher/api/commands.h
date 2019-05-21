#pragma once

#include "applauncher_api.h"

namespace nx::applauncher::api {

static constexpr int kDefaultTimeoutMs = 3000;

NX_VMS_APPLAUNCHER_API_API ResultType sendCommandToApplauncher(
    const api::BaseTask& commandToSend,
    api::Response* const response = nullptr,
    int timeoutMs = kDefaultTimeoutMs);

} // namespace nx::applauncher::api
