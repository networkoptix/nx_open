// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "poe_controller.h"

#include <functional>

#include <QtCore/QTimer>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/rest/params.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/desktop/system_context.h>

namespace {

static constexpr auto kNvrAction = "/api/nvrNetworkBlock";

} // namespace

namespace nx::vms::client::desktop {

static constexpr int kUpdateIntervalMs = 1000;

struct PoeController::Private: public QObject
{
    Private(PoeController* q);

    void setInitialUpdateInProgress(bool value);
    void setBlockData(const BlockData& data);
    void setServer(const QnMediaServerResourcePtr& value);
    void handleReply(
        bool success, rest::Handle currentHandle, const nx::network::rest::JsonResult& result);
    void update();
    void setPowered(const PoeController::PowerModes& value);
    void cancelRequests();

    PoeController* const q;

    QSet<rest::Handle> runningHandles;

    QnMediaServerResourcePtr server;
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

void PoeController::Private::setServer(const QnMediaServerResourcePtr& value)
{
    if (server == value)
        return;

    server = value;
    setBlockData(BlockData());

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
    runningHandles.remove(replyHandle);

    const nx::utils::ScopeGuard handleGuard([this]() { cancelRequests(); });
    setInitialUpdateInProgress(false);

    if (!success || result.error != nx::network::rest::Result::NoError)
        return;

    BlockData data;
    setBlockData(QJson::deserialize(result.reply, &data) ? data : BlockData());
}

void PoeController::Private::update()
{
    if (!server)
        return;

    auto systemContext = SystemContext::fromResource(server);
    if (!NX_ASSERT(systemContext))
        return;

    auto api = systemContext->connectedServerApi();
    if (!api)
        return;

    runningHandles << api->getJsonResult(
        kNvrAction,
        nx::network::rest::Params(),
        handleReplyCallback,
        QThread::currentThread(),
        server->getId());
}

void PoeController::Private::setPowered(const PoeController::PowerModes& value)
{
    if (!server)
        return;

    auto systemContext = SystemContext::fromResource(server);
    if (!NX_ASSERT(systemContext))
        return;

    auto api = systemContext->connectedServerApi();
    if (!api)
        return;

    runningHandles << api->postJsonResult(
        kNvrAction,
        nx::network::rest::Params(),
        QJson::serialized(value),
        handleReplyCallback,
        QThread::currentThread(),
        /*timeouts*/ {},
        server->getId());

    q->updatingPoweringModesChanged();
}

void PoeController::Private::cancelRequests()
{
    if (runningHandles.isEmpty())
        return;

    if (auto systemContext = SystemContext::fromResource(server))
    {
        if (auto api = systemContext->connectedServerApi())
        {
            for (const auto handle: runningHandles)
                api->cancelRequest(handle);
        }
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

void PoeController::setServer(const QnMediaServerResourcePtr& value)
{
    d->setServer(value);
}

QnMediaServerResourcePtr PoeController::server() const
{
    return d->server;
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
