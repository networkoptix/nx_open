// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_controller.h"

#include <functional>

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/rest/params.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/core/resource/resource_holder.h>

namespace {

static constexpr auto kNvrAction = "/api/nvrNetworkBlock";

} // namespace

namespace nx::vms::client::desktop {

static constexpr int kUpdateIntervalMs = 1000;

struct PoeController::Private:
    public QObject,
    public core::RemoteConnectionAware
{
    Private(PoeController* q);

    void setInitialUpdateInProgress(bool value);
    void setBlockData(const BlockData& data);
    void updateTargetResource(const QnUuid& value);
    void handleReply(
        bool success, rest::Handle currentHandle, const nx::network::rest::JsonResult& result);
    void update();
    void setPowered(const PoeController::PowerModes& value);
    void setUpdatingModeHandle(rest::Handle value);
    void resetHandles();
    void cancelRequest();

    PoeController* const q;

    rest::Handle handle = 0;
    rest::Handle updatingModeHandle = 0;

    core::ResourceHolder<QnMediaServerResource> serverHolder;
    bool autoUpdate = true;
    bool initialUpdateInProgress = false;
    PoeController::BlockData blockData;
    QTimer updateTimer;
    using Handler = std::function<void(
        bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)>;
    const Handler handleReplyCallback;
};

PoeController::Private::Private(PoeController* q):
    q(q),
    handleReplyCallback(nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            handleReply(success, handle, result);
        }))
{
    updateTimer.setInterval(kUpdateIntervalMs);
    connect(&updateTimer, &QTimer::timeout, this, &Private::update);
}

void PoeController::Private::setInitialUpdateInProgress(bool value)
{
    if (value == initialUpdateInProgress)
        return;

    initialUpdateInProgress = value;
    emit q->initialUpdateInProgressChanged();
}

void PoeController::Private::setBlockData(const BlockData& data)
{
    if (data == blockData)
        return;

    blockData = data;
    emit q->updated();
}

void PoeController::Private::updateTargetResource(const QnUuid& value)
{
    if (!serverHolder.setResourceId(value))
        return;

    setBlockData(BlockData());

    const auto server = serverHolder.resource();
    if (!server || !server->getServerFlags().testFlag(api::SF_HasPoeManagementCapability))
    {
        // Cleanup.
        updateTimer.stop();
        cancelRequest();
        return;
    }

    setInitialUpdateInProgress(true);
    update();
}

void PoeController::Private::handleReply(
    bool success, rest::Handle replyHandle, const nx::network::rest::JsonResult& result)
{
    if (handle != replyHandle)
        return; //< We just cancelled the request.

    setInitialUpdateInProgress(false);
    const nx::utils::ScopeGuard handleGuard([this]() { resetHandles(); });

    if (autoUpdate)
        updateTimer.start();

    if (!success || result.error != nx::network::rest::Result::NoError)
        return;

    BlockData data;
    setBlockData(QJson::deserialize(result.reply, &data) ? data : BlockData());
}

void PoeController::Private::update()
{
    if (!connection() || !serverHolder.resource() || handle != 0)
        return;

    updateTimer.stop();

    handle = connectedServerApi()->getJsonResult(
        kNvrAction,
        nx::network::rest::Params(),
        handleReplyCallback,
        QThread::currentThread(),
        serverHolder.resourceId());
}

void PoeController::Private::setPowered(const PoeController::PowerModes& value)
{
    if (!connection() || !serverHolder.resource())
        return;

    NX_ASSERT(updatingModeHandle == 0, "Behavior should be enforced on the UI level");

    updateTimer.stop();
    handle = connectedServerApi()->postJsonResult(
        kNvrAction,
        nx::network::rest::Params(),
        QJson::serialized(value),
        handleReplyCallback,
        QThread::currentThread(),
        /*timeouts*/ {},
        serverHolder.resourceId());

    setUpdatingModeHandle(handle);
}

void PoeController::Private::setUpdatingModeHandle(rest::Handle value)
{
    if (value == updatingModeHandle)
        return;

    updatingModeHandle = value;

    q->updatingPoweringModesChanged();
}

void PoeController::Private::resetHandles()
{
    setUpdatingModeHandle(0);
    handle = 0;
}

void PoeController::Private::cancelRequest()
{
    if (auto api = connectedServerApi(); api && handle > 0)
        api->cancelRequest(handle);
    resetHandles();
}

//--------------------------------------------------------------------------------------------------

PoeController::PoeController(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

PoeController::~PoeController()
{
    d->cancelRequest();
}

void PoeController::setResourceId(const QnUuid& value)
{
    d->updateTargetResource(value);
}

QnUuid PoeController::resourceId() const
{
    return d->serverHolder.resourceId();
}

const PoeController::BlockData& PoeController::blockData() const
{
    return d->blockData;
}

void PoeController::setAutoUpdate(bool value)
{
    if (d->autoUpdate == value)
        return;

    d->autoUpdate = value;

    if (d->autoUpdate)
    {
        d->update();
    }
    else
    {
        d->updateTimer.stop();
    }
}

bool PoeController::autoUpdate() const
{
    return d->autoUpdate;
}

void PoeController::setPowerModes(const PowerModes& value)
{
    d->setPowered(value);
}

bool PoeController::initialUpdateInProgress() const
{
    return d->initialUpdateInProgress;
}

bool PoeController::updatingPoweringModes() const
{
    return d->updatingModeHandle > 0;
}

bool PoeController::isNetworkRequestRunning() const
{
    return d->handle > 0;
}

void PoeController::cancelRequest()
{
    d->cancelRequest();
}

} // namespace nx::vms::client::desktop
