#pragma once

#include "applauncher_api.h"

namespace applauncher {
namespace api {

static constexpr int kDefaultTimeoutMs = 3000;

NX_VMS_APPLAUNCHER_API_API ResultType::Value sendCommandToApplauncher(
    const api::BaseTask& commandToSend,
    api::Response* const response = nullptr,
    int timeoutMs = kDefaultTimeoutMs);

} // namespace api
} // namespace applauncher
