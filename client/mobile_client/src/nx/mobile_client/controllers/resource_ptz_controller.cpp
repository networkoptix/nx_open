#include "resource_ptz_controller.h"

#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace client {
namespace mobile {

ResourcePtzController::ResourcePtzController(QObject* parent):
    base_type()
{
    connect(this, &ResourcePtzController::resourceIdChanged, this,
        [this]()
        {
            const auto resourceUuid = QnUuid::fromStringSafe(m_resourceId);
            const auto resource = qnResPool->getResourceById(resourceUuid);
            const auto controller = qnClientPtzPool->controller(resource);
            if (baseController() != controller)
                setBaseController(controller);
        });

    setParent(parent);
}

QString ResourcePtzController::resourceId() const
{
    return m_resourceId;
}

void ResourcePtzController::setResourceId(const QString& value)
{
    if (value == m_resourceId)
        return;

    m_resourceId = value;
    emit resourceIdChanged();
}

} // namespace mobile
} // namespace client
} // namespace nx

