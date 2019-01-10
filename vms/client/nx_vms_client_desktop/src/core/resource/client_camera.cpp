#include "client_camera.h"

#include <core/resource_management/resource_pool.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

#include <nx/vms/client/desktop/radass/radass_controller.h>

QnClientCameraResource::QnClientCameraResource(const QnUuid& resourceTypeId):
    base_type(resourceTypeId)
{
}

Qn::ResourceFlags QnClientCameraResource::flags() const
{
    Qn::ResourceFlags result = base_type::flags();
    if (!isDtsBased() && supportedMotionType() != Qn::MotionType::MT_NoMotion)
        result |= Qn::motion;

    return result;
}

void QnClientCameraResource::setAuthToCameraGroup(
    const QnVirtualCameraResourcePtr& camera,
    const QAuthenticator& authenticator)
{
    NX_ASSERT(camera->isMultiSensorCamera() || camera->isNvr());
    if (!camera->resourcePool())
        return;

    const auto groupId = camera->getGroupId();
    NX_ASSERT(!groupId.isEmpty());
    if (groupId.isEmpty())
        return;

    auto sensors = camera->resourcePool()->getResources<QnVirtualCameraResource>(
        [groupId](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->getGroupId() == groupId;
        });

    for (const auto& sensor: sensors)
    {
        sensor->setAuth(authenticator);
        sensor->savePropertiesAsync();
    }
}

QnAbstractStreamDataProvider* QnClientCameraResource::createDataProvider(
    const QnResourcePtr& resource,
    Qn::ConnectionRole role)
{
    const auto camera = resource.dynamicCast<QnClientCameraResource>();
    NX_ASSERT(camera && role == Qn::CR_Default);
    if (!camera)
        return nullptr;

     QnAbstractStreamDataProvider* result = camera->createLiveDataProvider();
     if (result)
         result->setRole(role);
     return result;
}

QnConstResourceVideoLayoutPtr QnClientCameraResource::getVideoLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    return QnVirtualCameraResource::getVideoLayout(dataProvider);
}

QnConstResourceAudioLayoutPtr QnClientCameraResource::getAudioLayout(
    const QnAbstractStreamDataProvider* dataProvider) const
{
    if (const auto archive = dynamic_cast<const QnArchiveStreamReader*>(dataProvider))
        return archive->getDPAudioLayout();

    return QnMediaResource::getAudioLayout();
}

QnAbstractStreamDataProvider* QnClientCameraResource::createLiveDataProvider()
{
    const auto camera = toSharedPointer(this);

    QnArchiveStreamReader *result = new QnArchiveStreamReader(camera);
    auto delegate = new QnRtspClientArchiveDelegate(result);
    delegate->setCamera(camera);
    result->setArchiveDelegate(delegate);

    connect(delegate, &QnRtspClientArchiveDelegate::dataDropped, this,
        &QnClientCameraResource::dataDropped);

    return result;
}
