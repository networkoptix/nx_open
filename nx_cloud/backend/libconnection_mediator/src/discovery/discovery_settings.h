#pragma once

#include <chrono>

class QnSettings;

namespace nx {
namespace cloud {
namespace discovery {
namespace conf {

class Discovery
{
public:
    std::chrono::milliseconds keepAliveTimeout;

    Discovery();

    void load(const QnSettings& settings);
};

} // namespace conf
} // namespace nx
} // namespace cloud
} // namespace discovery
