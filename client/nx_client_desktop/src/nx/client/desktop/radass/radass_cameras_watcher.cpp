#include "radass_cameras_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/client_camera.h>

#include <nx/client/desktop/radass/radass_controller.h>

#include <nx/streaming/archive_stream_reader.h>

namespace nx {
namespace client {
namespace desktop {

RadassCamerasWatcher::RadassCamerasWatcher(RadassController* controller,
    QnResourcePool* resourcePool,
    QObject* parent)
    :
    base_type(parent),
    m_radassController(controller)
{
    auto handleDataDropped =
        [this](QnArchiveStreamReader* reader)
        {
            if (m_radassController)
                m_radassController->onSlowStream(reader);
        };

    auto handleResourceAdded =
        [this, handleDataDropped](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<QnClientCameraResource>())
                connect(camera, &QnClientCameraResource::dataDropped, this, handleDataDropped);
        };

    auto handleResourceRemoved =
        [this](const QnResourcePtr& resource)
        {
            resource->disconnect(this);
        };

    connect(resourcePool, &QnResourcePool::resourceAdded, this, handleResourceAdded);
    connect(resourcePool, &QnResourcePool::resourceRemoved, this, handleResourceRemoved);

    for (const auto& resource: resourcePool->getResources())
        handleResourceAdded(resource);
}

RadassCamerasWatcher::~RadassCamerasWatcher()
{
}

} // namespace desktop
} // namespace client
} // namespace nx