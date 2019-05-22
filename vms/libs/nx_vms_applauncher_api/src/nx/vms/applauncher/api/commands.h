#pragma once

#include "applauncher_api.h"

namespace nx::vms::applauncher::api {

static constexpr int kDefaultTimeoutMs = 3000;

NX_VMS_APPLAUNCHER_API_API ResultType sendCommandToApplauncher(
    const api::BaseTask& commandToSend,
    api::Response* const response = nullptr,
    int timeoutMs = kDefaultTimeoutMs);

} // namespace nx::vms::applauncher::api
