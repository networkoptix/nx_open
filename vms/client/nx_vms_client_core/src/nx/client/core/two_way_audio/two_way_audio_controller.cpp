#include "two_way_audio_controller.h"

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
#include <nx/vms/client/core/common/utils/ordered_requests_helper.h>

namespace nx::vms::client::core {

struct TwoWayAudioController::Private
{
    Private(TwoWayAudioController* q);
    void setStarted(bool value);
    bool setActive(bool active, OperationCallback&& callback = OperationCallback());

    TwoWayAudioController* const q;
    OrderedRequestsHelper orderedRequestsHelper;
    QScopedPointer<TwoWayAudioAvailabilityWatcher> availabilityWatcher;
    QnVirtualCameraResourcePtr camera;
    QString sourceId;
    bool started = false;
    bool available = false;
};

TwoWayAudioController::Private::Private(TwoWayAudioController* q):
    q(q),
    availabilityWatcher(new TwoWayAudioAvailabilityWatcher())
{
    connect(availabilityWatcher, &TwoWayAudioAvailabilityWatcher::availabilityChanged,
        q, &TwoWayAudioController::availabilityChanged);
}

void TwoWayAudioController::Private::setStarted(bool value)
{
    if (started == value)
        return;

    started = value;
    emit q->startedChanged();
}

bool TwoWayAudioController::Private::setActive(bool active, OperationCallback&& callback)
{
    const auto serverId = q->commonModule()->remoteGUID();
    const auto server = q->resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    const bool available = server && server->getStatus() == Qn::Online && q->available();
    setStarted(active && available);
    if (!available)
        return false;

    const auto connection = server->restConnection();

    QnRequestParamList params;
    params.insert("clientId", sourceId);
    params.insert("resourceId", camera->getId().toString());
    params.insert("action", active ? "start" : "stop");

    const auto requestCallback = nx::utils::guarded(q,
        [this, active, callback](bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            const bool ok = success && result.error == QnRestResult::NoError;
            setStarted(active && ok);
            if (callback)
                callback(ok);
        });

    orderedRequestsHelper.getJsonResult(connection,
        "/api/transmitAudio", params, requestCallback, QThread::currentThread());
    return true;
}

//--------------------------------------------------------------------------------------------------

TwoWayAudioController::TwoWayAudioController(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
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
    return d->started;
}

bool TwoWayAudioController::start(OperationCallback&& callback)
{
    return !started() && d->setActive(true, std::move(callback));
}

bool TwoWayAudioController::start()
{
    return !started() && d->setActive(true);
}

void TwoWayAudioController::stop()
{
    if (started())
        d->setActive(false);
}

void TwoWayAudioController::setSourceId(const QString& value)
{
    if (d->sourceId == value)
        return;

    if (d->sourceId.isEmpty())
        stop();

    d->sourceId = value;
}

QString TwoWayAudioController::resourceId() const
{
    return d->camera ? d->camera->getId().toString() : QString();
}

void TwoWayAudioController::setResourceId(const QString& value)
{
    const auto id = QnUuid::fromStringSafe(value);
    if (d->camera && d->camera->getId() == id)
        return;

    stop();

    const auto pool = commonModule()->resourcePool();
    d->camera = pool->getResourceById<QnVirtualCameraResource>(id);
    d->availabilityWatcher->setResourceId(id);
    emit resourceIdChanged();
}

bool TwoWayAudioController::available() const
{
    return d->availabilityWatcher->available();
}

} // namespace nx::vms::client::core

