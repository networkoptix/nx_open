// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_core_camera.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource/camera_user_attribute_pool.h>

namespace nx::vms::client::core {

Camera::Camera(const QnUuid& resourceTypeId):
    base_type()
{
    setTypeId(resourceTypeId);
    addFlags(Qn::server_live_cam | Qn::depend_on_parent_status);
}

QString Camera::getName() const
{
    return getUserDefinedName();
}

void Camera::setName(const QString& name)
{
    if (getId().isNull())
    {
        base_type::setName(name);
        return;
    }

    {
        if (!commonModule())
            return;

        commonModule()->cameraUserAttributesPool()->setName(getId(), name);
    }
    emit nameChanged(toSharedPointer(this));
}

Qn::ResourceFlags Camera::flags() const {
    Qn::ResourceFlags result = base_type::flags();
    if (isIOModule())
        result |= Qn::io_module;
    return result;
}

nx::vms::api::ResourceStatus Camera::getStatus() const
{
    if (auto context = commonModule())
    {
        const auto serverStatus = context->resourceStatusDictionary()->value(getParentId());
        if (serverStatus == nx::vms::api::ResourceStatus::offline
            || serverStatus == nx::vms::api::ResourceStatus::unauthorized
            || serverStatus == nx::vms::api::ResourceStatus::mismatchedCertificate)
        {
            return nx::vms::api::ResourceStatus::offline;
        }
    }
    return QnResource::getStatus();
}

void Camera::setParentId(const QnUuid& parent)
{
    QnUuid oldValue = getParentId();
    if (oldValue != parent)
    {
        base_type::setParentId(parent);
        if (!oldValue.isNull())
            emit statusChanged(toSharedPointer(this), Qn::StatusChangeReason::Local);
    }
}

bool Camera::isPtzSupported() const
{
    // Camera must have at least one ptz control capability but fisheye must be disabled.
    return hasAnyOfPtzCapabilities(Ptz::ContinuousPtzCapabilities | Ptz::ViewportPtzCapability)
        && !hasAnyOfPtzCapabilities(Ptz::VirtualPtzCapability);
}

bool Camera::isPtzRedirected() const
{
    return !getProperty(ResourcePropertyKey::kPtzTargetId).isEmpty();
}

CameraPtr Camera::ptzRedirectedTo() const
{
    const auto redirectId = getProperty(ResourcePropertyKey::kPtzTargetId);
    if (redirectId.isEmpty() || !resourcePool())
        return {};

    return resourcePool()->getResourceByUniqueId<Camera>(redirectId);
}

void Camera::updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers)
{
    if (other->getParentId() != m_parentId)
    {
        notifiers <<
            [r = toSharedPointer(this)]
            {
                emit r->statusChanged(r, Qn::StatusChangeReason::Local);
            };
    }
    base_type::updateInternal(other, notifiers);
}

} // namespace nx::vms::client::core
