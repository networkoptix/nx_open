#pragma once

#include <nx/mediaserver/server_module_aware.h>

namespace nx {
namespace mediaserver {
namespace resource {

class AbstractSharedResourceContext: public ServerModuleAware
{
public:
    using SharedId = QString;

public:
    AbstractSharedResourceContext(
        QnMediaServerModule* serverModule,
        const SharedId& sharedId)
        :
        ServerModuleAware(serverModule)
    {};

    virtual ~AbstractSharedResourceContext() = default;
};

} // namespace resource
} // namespace mediaserver
} // namespace nx
