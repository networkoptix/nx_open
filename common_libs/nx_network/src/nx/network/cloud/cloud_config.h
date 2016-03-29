#pragma once

#include <chrono>

static const std::chrono::milliseconds kHpUdtConnectTimeout(15000);
static const std::chrono::milliseconds kHpUdtKeepAliveInterval(15000);
static const size_t kHpUdtKeepAliveRetries(3);
