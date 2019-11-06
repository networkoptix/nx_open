#include "poe_controller.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <utils/common/delayed.h>
#include <nx/vms/client/core/resource/resource_holder.h>

namespace {

static constexpr auto kNvrAction = "/api/nvrNetworkBlock";

} // namespace

namespace nx::vms::client::core {

static constexpr int kUpdateIntervalMs = 1000;

struct PoEController::Private: public QObject
{
    Private(PoEController* q);

    void updateTargetResource(const QnUuid& value);
    void setBlockData(const OptionalBlockData& value);
    void update();

    void setPowered(const PoEController::PowerModes& value);
    \
    PoEController* const q;

    rest::QnConnectionPtr connection;
    rest::Handle handle = -1;

    ResourceHolder<QnMediaServerResource> serverHolder;
    bool autoUpdate = true;
    PoEController::OptionalBlockData blockData;
    QTimer updateTimer;
};

PoEController::Private::Private(PoEController* q):
    q(q)
{
    updateTimer.setInterval(kUpdateIntervalMs);
    connect(&updateTimer, &QTimer::timeout, this, &Private::update);
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
        handle = -1;
        return;
    }

    connection = server->restConnection();
    update();
}

void PoEController::Private::setBlockData(const OptionalBlockData& value)
{
    blockData = value;
    emit q->updated();
}

void PoEController::Private::update()
{
    if (!connection)
        return;

    const auto callback =
        [this](bool success, rest::Handle currentHandle, const QnJsonRestResult& result)
        {
            if (handle != currentHandle)
                return;

            handle = -1;

            if (autoUpdate)
                updateTimer.start();

            if (!success || result.error != QnRestResult::NoError)
                return;

            BlockData data;
            if (QJson::deserialize(result.reply, &data))
                setBlockData(data);

            static int k = 0;
            PowerModes modes;
            ++k;
            for (int i = 0; i != 8; ++i)
            {
                PortPoweringMode mode;
                mode.portNumber = i + 1;
                mode.poweringMode = k % 2 ? PortPoweringMode::PoweringMode::on : PortPoweringMode::PoweringMode::off;
                modes.append(mode);
            }
            setPowered(modes);
        };

    updateTimer.stop();

    handle = connection->getJsonResult(
        kNvrAction, QnRequestParamList(), callback, QThread::currentThread());
}

void PoEController::Private::setPowered(const PoEController::PowerModes& value)
{
    if (!connection)
        return;

    const auto callback =
        [this](bool success, rest::Handle currentHandle, const QnJsonRestResult& result)
        {
        };
    connection->postJsonResult(
        kNvrAction, QnRequestParamList(), QJson::serialized(value), callback, QThread::currentThread());
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

PoEController::OptionalBlockData PoEController::blockData() const
{
    return d->blockData;
}

void PoEController::setAutoUpdate(bool value)
{
    if (d->autoUpdate == value)
        return;

    d->autoUpdate = value;

    if (d->autoUpdate)
        d->update();
    else
        d->updateTimer.stop();
}

bool PoEController::autoUpdate() const
{
    return d->autoUpdate;
}

void PoEController::setPowered(const PowerModes& value)
{
    d->setPowered(value);
}

} // namespace nx::vms::client::core

