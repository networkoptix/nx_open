#pragma once

class QnSettings;

namespace nx {
namespace cdb {
namespace ec2 {

class Settings
{
public:
    unsigned int maxConcurrentConnectionsFromSystem;

    Settings();

    void load(const QnSettings& settings);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
