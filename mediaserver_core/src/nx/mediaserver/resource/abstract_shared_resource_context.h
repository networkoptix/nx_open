#pragma once

namespace nx {
namespace mediaserver {
namespace resource {

class AbstractSharedResourceContext
{
public:
    using SharedId = QString;

public:
    virtual ~AbstractSharedResourceContext() = default;
};

} // namespace resource
} // namespace mediaserver
} // namespace nx
