#pragma once

namespace nx {
namespace vms::server {
namespace resource {

class AbstractSharedResourceContext
{
public:
    using SharedId = QString;

public:
    virtual ~AbstractSharedResourceContext() = default;
};

} // namespace resource
} // namespace vms::server
} // namespace nx
