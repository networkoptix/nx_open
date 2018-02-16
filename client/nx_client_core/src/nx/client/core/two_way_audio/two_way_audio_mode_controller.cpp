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
    base_type(parent),
    QnConnectionContextAware()
{
}

TwoWayAudioController::TwoWayAudioController(
    const QString& sourceId,
    const QString& cameraId,
    QObject* parent)
    :
    base_type(parent),
    QnConnectionContextAware(),
    m_camera()
{
    setSourceId(sourceId);
    setResourceId(cameraId);
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
    if (!server || server->getStatus() != Qn::Online || !available())
        return false;

    setStarted(true); // Intermediate state.
    const auto connection = server->restConnection();
    m_startHandle = connection->twoWayAudioCommand(m_sourceId, m_camera->getId(), true,
        [this, guard = QPointer<TwoWayAudioController>(this)]
            (bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            qDebug() << "DDD-----------" << success << result.error << result.errorString;
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

void TwoWayAudioController::setSourceId(const QString& value)
{
    if (m_sourceId == value)
        return;

    if (m_sourceId.isEmpty())
        stop();

    m_sourceId = value;
}

QString TwoWayAudioController::resourceId() const
{
    return m_camera ? m_camera->getId().toString() : QString();
}

void TwoWayAudioController::setResourceId(const QString& value)
{
    const auto id = QnUuid::fromStringSafe(value);
    if (m_camera && m_camera->getId() == id)
        return;

    stop();
    if (m_camera)
        m_camera->disconnect(this);

    const auto pool = resourcePool();
    m_camera = pool->getResourceById<QnVirtualCameraResource>(id);
    m_licenseHelper.reset(m_camera && m_camera->isIOModule()
        ? new QnSingleCamLicenseStatusHelper(m_camera)
        : nullptr);

    if (m_camera)
    {
        connect(m_camera, &QnVirtualCameraResource::statusChanged,
            this, &TwoWayAudioController::updateAvailability);
        if (m_licenseHelper)
        {
            connect(m_licenseHelper, &QnSingleCamLicenseStatusHelper::licenseStatusChanged,
                this, &TwoWayAudioController::updateAvailability);
        }
    }

    updateAvailability();
    emit resourceIdChanged();
}

bool TwoWayAudioController::available() const
{
    return m_available;
}

void TwoWayAudioController::updateAvailability()
{
    const bool isAvailable =
        [this]()
        {
            if (!m_camera)
                return false;

            if (m_camera->getStatus() != Qn::Online && m_camera->getStatus() != Qn::Recording)
                return false;

            if (m_licenseHelper)
                return m_licenseHelper->status() == QnLicenseUsageStatus::used;

            return true;
        }();

    setAvailability(isAvailable);
}

void TwoWayAudioController::setAvailability(bool value)
{
    if (m_available == value)
        return;

    m_available = value;
    emit availabilityChanged();
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

