#pragma once

class QnSettings;

namespace nx {
namespace cdb {
namespace ec2 {

class NX_DATA_SYNC_ENGINE_API Settings
{
public:
    unsigned int maxConcurrentConnectionsFromSystem;

    Settings();

    void load(const QnSettings& settings);
};

} // namespace ec2
} // namespace cdb
} // namespace nx
