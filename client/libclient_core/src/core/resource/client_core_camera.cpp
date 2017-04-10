#include "client_core_camera.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource/camera_user_attribute_pool.h>

QnClientCoreCamera::QnClientCoreCamera(const QnUuid& resourceTypeId):
    base_type()
{
    setTypeId(resourceTypeId);
    addFlags(Qn::server_live_cam | Qn::depend_on_parent_status);
}

QString QnClientCoreCamera::getDriverName() const {
    return QLatin1String("Server camera"); //all other manufacture are also untranslated and should not be translated
}

QString QnClientCoreCamera::getName() const {
    return getUserDefinedName();
}

void QnClientCoreCamera::setName(const QString& name)
{
    if (getId().isNull())
    {
        base_type::setName(name);
        return;
    }

    {
        if (!commonModule())
            return;

        QnCameraUserAttributePool::ScopedLock userAttributesLock(
            commonModule()->cameraUserAttributesPool(), getId());

        if ((*userAttributesLock)->name == name)
            return;
        (*userAttributesLock)->name = name;
    }
    emit nameChanged(toSharedPointer(this));
}


Qn::ResourceFlags QnClientCoreCamera::flags() const {
    Qn::ResourceFlags result = base_type::flags();
    if (isIOModule())
        result |= Qn::io_module;
    return result;
}

void QnClientCoreCamera::setIframeDistance(int frames, int timems) {
    Q_UNUSED(frames)
    Q_UNUSED(timems)
}

Qn::ResourceStatus QnClientCoreCamera::getStatus() const
{
    if (auto context = commonModule())
    {
        Qn::ResourceStatus serverStatus =
            context->statusDictionary()->value(getParentId());
        if (serverStatus == Qn::Offline || serverStatus == Qn::Unauthorized)
            return Qn::Offline;
    }
    return QnResource::getStatus();
}

void QnClientCoreCamera::setParentId(const QnUuid& parent) {
    QnUuid oldValue = getParentId();
    if (oldValue != parent) {
        base_type::setParentId(parent);
        if (!oldValue.isNull())
            emit statusChanged(toSharedPointer(this), Qn::StatusChangeReason::Local);
    }
}

void QnClientCoreCamera::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    if (other->getParentId() != m_parentId)
        notifiers << [r = toSharedPointer(this)]{emit r->statusChanged(r, Qn::StatusChangeReason::Local);};
    base_type::updateInternal(other, notifiers);
}

