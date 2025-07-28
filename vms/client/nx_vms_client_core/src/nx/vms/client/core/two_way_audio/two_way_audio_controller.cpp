// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "two_way_audio_controller.h"

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/common/utils/ordered_requests_helper.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_availability_watcher.h>
#include <nx/vms/client/core/two_way_audio/two_way_audio_streamer.h>

namespace nx::vms::client::core {

struct TwoWayAudioController::Private
{
    Private(TwoWayAudioController* q);
    void setStarted(bool value);
    bool setActive(bool active, OperationCallback&& callback = OperationCallback());
    bool setActive_6_0(bool active, OperationCallback&& callback = OperationCallback());
    bool setActiveActual(bool active, OperationCallback&& callback = OperationCallback());

    TwoWayAudioController* const q;
    OrderedRequestsHelper orderedRequestsHelper;
    QScopedPointer<TwoWayAudioAvailabilityWatcher> availabilityWatcher;
    std::shared_ptr<TwoWayAudioStreamer> streamer;
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
    connect(availabilityWatcher.get(), &TwoWayAudioAvailabilityWatcher::audioOutputDeviceChanged, q,
        [q]() { q->stop(); });
    connect(availabilityWatcher.get(), &TwoWayAudioAvailabilityWatcher::cameraChanged,
        q, &TwoWayAudioController::resourceIdChanged);
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
    const bool available = q->connection() && q->available();
    setStarted(active && available);
    if (!available)
        return false;

    const auto serverVersion = q->connection()->moduleInformation().version;
    if (serverVersion < nx::utils::SoftwareVersion(6, 1))
        return setActive_6_0(active, std::move(callback)); // To support servers 6.0 and before.
    else
        return setActiveActual(active, std::move(callback));
}

bool TwoWayAudioController::Private::setActiveActual(bool active, OperationCallback&& callback)
{
    const auto targetResource = availabilityWatcher->audioOutputDevice();
    if (!NX_ASSERT(targetResource))
        return false;

    if (active)
    {
        auto provider = appContext()->createAudioInputProvider();
        if (!provider)
        {
            NX_WARNING(this, "Failed to create desktop data provider");
            return false;
        }
        streamer = std::make_shared<TwoWayAudioStreamer>(std::move(provider));
        const bool result = streamer->start(q->connection()->credentials(), targetResource);
        setStarted(result);
        if (callback)
            callback(result);
    }
    else
    {
        setStarted(false);
        if (streamer)
        {
            streamer->stop();
            streamer.reset();
        }
        if (callback)
            callback(true);
    }
    return true;
}

bool TwoWayAudioController::Private::setActive_6_0(bool active, OperationCallback&& callback)
{
    const auto targetResource = availabilityWatcher->audioOutputDevice();
    if (!NX_ASSERT(targetResource))
        return false;

    nx::network::rest::Params params;
    params.insert("clientId", sourceId);
    params.insert("resourceId", targetResource->getId().toSimpleString());
    params.insert("action", active ? "start" : "stop");

    const auto requestCallback = nx::utils::guarded(q,
        [this, active, callback](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            const bool ok = success && result.errorId == nx::network::rest::ErrorId::ok;
            setStarted(active && ok);
            if (callback)
                callback(ok);
        });

    return orderedRequestsHelper.postJsonResult(q->connectedServerApi(),
        "/api/transmitAudio", params, requestCallback, QThread::currentThread());
}

//--------------------------------------------------------------------------------------------------

TwoWayAudioController::TwoWayAudioController(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
}

TwoWayAudioController::~TwoWayAudioController()
{
    stop();
}

void TwoWayAudioController::registerQmlType()
{
    qmlRegisterUncreatableType<TwoWayAudioController>(
        "nx.vms.client.core", 1, 0, "TwoWayAudioController",
        "Cannot create an instance of TwoWayAudioController without a system context.");
}

bool TwoWayAudioController::started() const
{
    return d->started;
}

bool TwoWayAudioController::start(OperationCallback&& callback)
{
    return !started() && d->setActive(true, std::move(callback));
}

bool TwoWayAudioController::stop(OperationCallback&& callback)
{
    return started() && d->setActive(false, std::move(callback));
}

void TwoWayAudioController::setSourceId(const QString& value)
{
    if (d->sourceId == value)
        return;

    if (d->sourceId.isEmpty())
        stop();

    d->sourceId = value;
}

nx::Uuid TwoWayAudioController::resourceId() const
{
    const auto camera = d->availabilityWatcher->camera();
    return camera ? camera->getId() : nx::Uuid();
}

void TwoWayAudioController::setResourceId(const nx::Uuid& id)
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    d->availabilityWatcher->setCamera(camera);
}

void TwoWayAudioController::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->availabilityWatcher->setCamera(camera);
}

bool TwoWayAudioController::available() const
{
    return d->availabilityWatcher->available();
}

} // namespace nx::vms::client::core
