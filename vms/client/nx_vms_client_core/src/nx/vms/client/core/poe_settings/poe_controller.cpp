#include "poe_controller.h"

#include <QtCore/QTimer>

#include <functional>

#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/resource/resource_holder.h>

namespace {

static constexpr auto kNvrAction = "/api/nvrNetworkBlock";

} // namespace

namespace nx::vms::client::core {

using namespace std::placeholders;

static constexpr int kUpdateIntervalMs = 3000;

struct PoEController::Private: public QObject
{
    Private(PoEController* q);

    void setInitialUpdateInProgress(bool value);
    void setBlockData(const BlockData& data);
    void updateTargetResource(const QnUuid& value);
    void handleReply(bool success, rest::Handle currentHandle, const QnJsonRestResult& result);
    void update();
    void setPowered(const PoEController::PowerModes& value);
    void setUpdatingModeHandle(rest::Handle value);
    void resetHandles();

    PoEController* const q;

    rest::QnConnectionPtr connection;
    rest::Handle handle = -1;
    rest::Handle updatingModeHandle = -1;

    ResourceHolder<QnMediaServerResource> serverHolder;
    bool autoUpdate = true;
    bool initialUpdateInProgress = false;
    PoEController::BlockData blockData;
    QTimer updateTimer;
};

PoEController::Private::Private(PoEController* q):
    q(q)
{
    updateTimer.setInterval(kUpdateIntervalMs);
    connect(&updateTimer, &QTimer::timeout, this, &Private::update);
}

void PoEController::Private::setInitialUpdateInProgress(bool value)
{
    if (value == initialUpdateInProgress)
        return;

    initialUpdateInProgress = value;
    emit q->initialUpdateInProgressChanged();
}

void PoEController::Private::setBlockData(const BlockData& data)
{
    if (data == blockData)
        return;

    blockData = data;
    emit q->updated();
}

void PoEController::Private::updateTargetResource(const QnUuid& value)
{
    if (!serverHolder.setResourceId(value))
        return;

    const auto server = serverHolder.resource();
    if (!server)
    {
        // Cleanup.
        connection.reset();
        updateTimer.stop();
        resetHandles();
        return;
    }

    connection = server->restConnection();
    setInitialUpdateInProgress(true);
    setBlockData(BlockData());
    update();
}

void PoEController::Private::handleReply(
    bool success,
    rest::Handle replyHandle,
    const QnJsonRestResult& result)
{
    if (handle != replyHandle)
        return; //< Most likely we cancelled request

    setInitialUpdateInProgress(false);
    const nx::utils::ScopeGuard handleGuard([this]() { resetHandles(); });

    if (autoUpdate)
        updateTimer.start();

    if (!success || result.error != QnRestResult::NoError)
        return;

    BlockData data;
    setBlockData(QJson::deserialize(result.reply, &data) ? data : BlockData());
}

void PoEController::Private::update()
{
    if (!connection)
        return;

    updateTimer.stop();

    handle = connection->getJsonResult(
        kNvrAction,
        QnRequestParamList(),
        std::bind(&Private::handleReply, this, _1, _2, _3),
        QThread::currentThread());
}

void PoEController::Private::setPowered(const PoEController::PowerModes& value)
{
    if (!connection)
        return;

    updateTimer.stop();
    handle = connection->postJsonResult(
        kNvrAction,
        QnRequestParamList(),
        QJson::serialized(value),
        std::bind(&Private::handleReply, this, _1, _2, _3),
        QThread::currentThread());

    setUpdatingModeHandle(handle);
}

void PoEController::Private::setUpdatingModeHandle(rest::Handle value)
{
    if (value == updatingModeHandle)
        return;

    updatingModeHandle = value;

    q->updatingPoweringModesChanged();
}

void PoEController::Private::resetHandles()
{
    setUpdatingModeHandle(-1);
    handle = -1;
}

//--------------------------------------------------------------------------------------------------

PoEController::PoEController(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

PoEController::~PoEController()
{
}

void PoEController::setResourceId(const QnUuid& value)
{
    d->updateTargetResource(value);
}

QnUuid PoEController::resourceId() const
{
    return d->serverHolder.resourceId();
}

const PoEController::BlockData& PoEController::blockData() const
{
    return d->blockData;
}

void PoEController::setAutoUpdate(bool value)
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

bool PoEController::autoUpdate() const
{
    return d->autoUpdate;
}

void PoEController::setPowerModes(const PowerModes& value)
{
    d->setPowered(value);
}

bool PoEController::initialUpdateInProgress() const
{
    return d->initialUpdateInProgress;
}

bool PoEController::updatingPoweringModes() const
{
    return d->updatingModeHandle >= 0;
}

} // namespace nx::vms::client::core

