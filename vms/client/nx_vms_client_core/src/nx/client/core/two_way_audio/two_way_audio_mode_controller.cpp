#include "two_way_audio_mode_controller.h"

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>

#include <nx/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

TwoWayAudioController::TwoWayAudioController(QObject* parent):
    base_type(parent),
    m_availabilityWatcher(new TwoWayAudioAvailabilityWatcher())
{
    connect(m_availabilityWatcher, &TwoWayAudioAvailabilityWatcher::availabilityChanged,
        this, &TwoWayAudioController::availabilityChanged);
}

TwoWayAudioController::TwoWayAudioController(
    const QString& sourceId,
    const QString& cameraId,
    QObject* parent)
    :
    base_type(parent),
    m_availabilityWatcher(new TwoWayAudioAvailabilityWatcher())
{
    connect(m_availabilityWatcher, &TwoWayAudioAvailabilityWatcher::availabilityChanged,
        this, &TwoWayAudioController::availabilityChanged);

    setSourceId(sourceId);
    setResourceId(cameraId);
    start();
}

TwoWayAudioController::~TwoWayAudioController()
{
    stop();
}

void TwoWayAudioController::registerQmlType()
{
    qmlRegisterType<TwoWayAudioController>("nx.client.core", 1, 0, "TwoWayAudioController");
}

bool TwoWayAudioController::started() const
{
    return m_started;
}

bool TwoWayAudioController::start()
{
    if (started() || !available())
        return false;

    const auto serverId = commonModule()->remoteGUID();
    const auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server || server->getStatus() != Qn::Online || !available())
        return false;

    setStarted(true); // Intermediate state.

    const auto callback =
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (handle == m_startHandle)
                setStarted(success && result.error == QnRestResult::NoError);

        };
    const auto connection = server->restConnection();
    m_startHandle = connection->twoWayAudioCommand(m_sourceId, m_camera->getId(), true,
        nx::utils::guarded(this, callback), QThread::currentThread());

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

    const auto pool = commonModule()->resourcePool();
    m_camera = pool->getResourceById<QnVirtualCameraResource>(id);
    m_availabilityWatcher->setResourceId(id);
    emit resourceIdChanged();
}

bool TwoWayAudioController::available() const
{
    return m_availabilityWatcher->available();
}

void TwoWayAudioController::setStarted(bool value)
{
    if (m_started == value)
        return;

    m_started = value;
    emit startedChanged();
}

} // namespace nx::vms::client::core

