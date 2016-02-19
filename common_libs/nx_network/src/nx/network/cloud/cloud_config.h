#pragma once

#include <chrono>

static const std::chrono::milliseconds kHpUdtConnectTimeout(3000);
static const std::chrono::milliseconds kHpUdtKeepAliveInterval(3000);
static const size_t kHpUdtKeepAliveRetries(3);
