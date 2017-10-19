#pragma once

namespace nx {
namespace mediaserver {
namespace resource {

class AbstractSharedResourceContext
{
public:
    using SharedId = QString;

public:
    AbstractSharedResourceContext() = default;
    virtual ~AbstractSharedResourceContext() = default;
};

} // namespace resource
} // namespace mediaserver
} // namespace nx
