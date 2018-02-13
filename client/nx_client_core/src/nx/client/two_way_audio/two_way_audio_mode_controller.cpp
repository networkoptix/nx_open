#include "two_way_audio_mode_controller.h"

#include <nx/utils/log/assert.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>

namespace nx {
namespace client {
namespace core {

TwoWayAudioController::TwoWayAudioController(QObject* parent):
    QObject(parent),
    base_type()
{
}

TwoWayAudioController::TwoWayAudioController(
    const QString& sourceId,
    const QUuid& cameraId,
    QObject* parent)
    :
    QObject(parent),
    base_type(),
    m_camera()
{
    setStreamData(sourceId, cameraId);
    start();
}

TwoWayAudioController::~TwoWayAudioController()
{
    stop();
}

bool TwoWayAudioController::started() const
{
    return m_started;
}

bool TwoWayAudioController::start()
{
    if (started())
        return false;

    const auto serverId = commonModule()->remoteGUID();
    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server || server->getStatus() != Qn::Online || !isAllowed())
        return false;

    setStarted(true); // Intermediate state.
    const auto connection = server->restConnection();
    m_startHandle = connection->twoWayAudioCommand(m_sourceId, m_camera->getId(), true,
        [this, guard = QPointer<TwoWayAudioController>(this)]
            (bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (guard && handle == m_startHandle)
                setStarted(success && result.error == QnRestResult::NoError);

        }, QThread::currentThread());

    return true;
}

void TwoWayAudioController::stop()
{
    if (!started())
        return;

    setStarted(false);

    const auto serverId = commonModule()->remoteGUID();
    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server || server->getStatus() != Qn::Online)
        return;

    const auto connection = server->restConnection();
    connection->twoWayAudioCommand(m_sourceId, m_camera->getId(), false,
        rest::ServerConnection::GetCallback());
}

bool TwoWayAudioController::setStreamData(
    const QString& sourceId,
    const QUuid& cameraId)
{
    if (sourceId.isEmpty() || cameraId.isNull())
        return false;

    const auto pool = resourcePool();
    m_camera = pool->getResourceById<QnVirtualCameraResource>(cameraId);
    m_sourceId = sourceId;
//            QnDesktopResource::calculateUniqueId(
//        commonModule()->moduleGUID(), user->getId());
    m_licenseHelper.reset(m_camera && m_camera->isIOModule()
        ? new QnSingleCamLicenseStatusHelper(m_camera)
        : nullptr);

    return m_camera;
}

bool TwoWayAudioController::isAllowed() const
{
    if (!m_camera)
        return false;

    if (m_camera->getStatus() != Qn::Online && m_camera->getStatus() != Qn::Recording)
        return false;

    if (m_licenseHelper)
        return m_licenseHelper->status() == QnLicenseUsageStatus::used;

    return true;
}

void TwoWayAudioController::setStarted(bool value)
{
    if (m_started == value)
        return;

    m_started = value;
    emit startedChanged();
}

} // namespace core
} // namespace client
} // namespace nx

