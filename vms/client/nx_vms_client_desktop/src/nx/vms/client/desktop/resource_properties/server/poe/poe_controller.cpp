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
    void updateTargetResource(const nx::Uuid& value);
    void handleReply(
        bool success, rest::Handle currentHandle, const nx::network::rest::JsonResult& result);
    void update();
    void setPowered(const PoeController::PowerModes& value);
    void cancelRequests();

    PoeController* const q;

    QSet<rest::Handle> runningHandles;

    core::ResourceHolder<QnMediaServerResource> serverHolder;
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
    updateTimer.start();
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

void PoeController::Private::updateTargetResource(const nx::Uuid& value)
{
    if (!serverHolder.setResourceId(value))
        return;

    setBlockData(BlockData());

    const auto server = serverHolder.resource();
    if (!server || !server->getServerFlags().testFlag(api::SF_HasPoeManagementCapability))
    {
        // Cleanup.
        cancelRequests();
        return;
    }

    setInitialUpdateInProgress(true);
    update();
}

void PoeController::Private::handleReply(
    bool success, rest::Handle replyHandle, const nx::network::rest::JsonResult& result)
{
    auto api = connectedServerApi();
    if (!api)
        return;

    runningHandles.remove(replyHandle);
    for (const auto handle: runningHandles)
        api->cancelRequest(handle);
    runningHandles.clear();

    setInitialUpdateInProgress(false);
    const nx::utils::ScopeGuard handleGuard([this]() { cancelRequests(); });

    if (!success || result.error != nx::network::rest::Result::NoError)
        return;

    BlockData data;
    setBlockData(QJson::deserialize(result.reply, &data) ? data : BlockData());
}

void PoeController::Private::update()
{
    auto api = connectedServerApi();
    if (!api || !serverHolder.resource())
        return;

    runningHandles << api->getJsonResult(
        kNvrAction,
        nx::network::rest::Params(),
        handleReplyCallback,
        QThread::currentThread(),
        serverHolder.resourceId());
}

void PoeController::Private::setPowered(const PoeController::PowerModes& value)
{
    auto api = connectedServerApi();
    if (!api || !serverHolder.resource())
        return;

    runningHandles << api->postJsonResult(
        kNvrAction,
        nx::network::rest::Params(),
        QJson::serialized(value),
        handleReplyCallback,
        QThread::currentThread(),
        /*timeouts*/ {},
        serverHolder.resourceId());

    q->updatingPoweringModesChanged();
}

void PoeController::Private::cancelRequests()
{
    if (runningHandles.isEmpty())
        return;

    if (auto api = connectedServerApi(); api)
    {
        for (const auto handle: runningHandles)
            connectedServerApi()->cancelRequest(handle);
    }
    runningHandles.clear();
    q->updatingPoweringModesChanged();
}

//--------------------------------------------------------------------------------------------------

PoeController::PoeController(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

PoeController::~PoeController()
{
    d->cancelRequests();
}

void PoeController::setResourceId(const nx::Uuid& value)
{
    d->updateTargetResource(value);
}

nx::Uuid PoeController::resourceId() const
{
    return d->serverHolder.resourceId();
}

const PoeController::BlockData& PoeController::blockData() const
{
    return d->blockData;
}

void PoeController::setAutoUpdate(bool value)
{
    if (value)
    {
        d->update();
        d->updateTimer.start();
    }
    else
    {
        d->updateTimer.stop();
    }
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
    return !d->runningHandles.isEmpty();
}

void PoeController::cancelRequest()
{
    d->cancelRequests();
}

} // namespace nx::vms::client::desktop
