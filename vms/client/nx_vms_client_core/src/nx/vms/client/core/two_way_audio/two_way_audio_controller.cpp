// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_controller.h"

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/common/utils/ordered_requests_helper.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_availability_watcher.h>

namespace nx::vms::client::core {

struct TwoWayAudioController::Private: RemoteConnectionAware
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
    connect(availabilityWatcher.get(), &TwoWayAudioAvailabilityWatcher::availabilityChanged,
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
    const bool available = connection() && q->available();
    setStarted(active && available);
    if (!available)
        return false;

    nx::network::rest::Params params;
    params.insert("clientId", sourceId);
    params.insert("resourceId", camera->getId().toString());
    params.insert("action", active ? "start" : "stop");

    const auto requestCallback = nx::utils::guarded(q,
        [this, active, callback](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            const bool ok = success && result.error == nx::network::rest::Result::NoError;
            setStarted(active && ok);
            if (callback)
                callback(ok);
        });

    orderedRequestsHelper.postJsonResult(connectedServerApi(),
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

QnUuid TwoWayAudioController::resourceId() const
{
    return d->camera ? d->camera->getId() : QnUuid();
}

void TwoWayAudioController::setResourceId(const QnUuid& id)
{
    if (d->camera && d->camera->getId() == id)
        return;

    stop();

    const auto pool = resourcePool();
    d->camera = pool->getResourceById<QnVirtualCameraResource>(id);
    d->availabilityWatcher->setResourceId(id);
    emit resourceIdChanged();
}

bool TwoWayAudioController::available() const
{
    return d->availabilityWatcher->available();
}

} // namespace nx::vms::client::core
