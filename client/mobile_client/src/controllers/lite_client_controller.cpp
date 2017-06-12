#include "lite_client_controller.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <api/runtime_info_manager.h>
#include <api/server_rest_connection.h>
#include <api/app_server_connection.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <managers/videowall_manager.h>
#include <utils/common/id.h>
#include <helpers/lite_client_layout_helper.h>

namespace {

    const int kStartStopTimeoutMs = 60 * 1000;

} // namespace

class QnLiteClientControllerPrivate: public QObject, public QnConnectionContextAware
{
    Q_DECLARE_PUBLIC(QnLiteClientController)
    QnLiteClientController* q_ptr;

public:
    QnLiteClientLayoutHelper* layoutHelper;
    QTimer* startStopTimer;
    QnMediaServerResourcePtr server;
    QnUuid serverId;
    bool serverOnline = false;
    QnLiteClientController::State clientState = QnLiteClientController::State::Stopped;
    rest::Handle clientStartHandle = -1;

public:
    QnLiteClientControllerPrivate(QnLiteClientController* parent);

    void setClientState(QnLiteClientController::State state);

    void setClientStartResult(bool success);
    void setClientStopResult(bool success);

    void updateServerStatus();

    QnLayoutResourcePtr createLayout() const;
    QnLayoutResourcePtr findLayout() const;
    bool initLayout();

    void at_runtimeInfoAdded(const QnPeerRuntimeInfo& data);
    void at_runtimeInfoRemoved(const QnPeerRuntimeInfo& data);
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& data);
    void at_startStopTimer_timeout();
};

QnLiteClientController::QnLiteClientController(QObject* parent):
    base_type(parent),
    d_ptr(new QnLiteClientControllerPrivate(this))
{
}

QnLiteClientController::~QnLiteClientController()
{
}

QString QnLiteClientController::serverId() const
{
    Q_D(const QnLiteClientController);
    return !d->serverId.isNull() ? d->serverId.toString() : QString();
}

void QnLiteClientController::setServerId(const QString& serverId)
{
    Q_D(QnLiteClientController);

    const auto id = QnUuid::fromStringSafe(serverId);

    if (d->serverId == id)
        return;

    d->server = resourcePool()->getResourceById<QnMediaServerResource>(id);
    emit serverIdChanged();

    if (d->server)
    {
        d->serverId = d->server->getId();

        connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded,
            d, &QnLiteClientControllerPrivate::at_runtimeInfoAdded);
        connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved,
            d, &QnLiteClientControllerPrivate::at_runtimeInfoRemoved);

        connect(d->server, &QnMediaServerResource::statusChanged,
            d, &QnLiteClientControllerPrivate::updateServerStatus);

        const auto items = runtimeInfoManager()->items()->getItems();
        const auto it = std::find_if(items.begin(), items.end(),
            [d](const QnPeerRuntimeInfo& info)
            {
                // Assuming a Mobile Client with proper videowallInstanceGuid is a Lite Client.
                return info.data.peer.peerType == Qn::PT_MobileClient
                    && info.data.videoWallInstanceGuid == d->serverId;
            });
        d->setClientState(it != items.end() ? State::Started : State::Stopped);
        d->initLayout();
    }
    else
    {
        disconnect(runtimeInfoManager(), nullptr, this, nullptr);
        d->setClientState(State::Stopped);
    }

    d->updateServerStatus();
}

QnLiteClientController::State QnLiteClientController::clientState() const
{
    Q_D(const QnLiteClientController);
    return d->clientState;
}

bool QnLiteClientController::isServerOnline() const
{
    Q_D(const QnLiteClientController);
    return d->serverOnline;
}

QnLiteClientLayoutHelper*QnLiteClientController::layoutHelper() const
{
    Q_D(const QnLiteClientController);
    return d->layoutHelper;
}

void QnLiteClientController::startLiteClient()
{
    Q_D(QnLiteClientController);

    if (d->clientState == State::Started || d->clientState == State::Starting)
        return;

    if (!d->server)
        return;

    d->setClientState(State::Starting);

    if (!d->initLayout())
    {
        d->setClientStartResult(false);
        return;
    }

    auto handleReply = [this, d](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            Q_UNUSED(result);

            if (d->clientStartHandle != handle)
                return;

            if (!success || result.error != QnJsonRestResult::NoError)
                d->setClientStartResult(false);
        };

    d->clientStartHandle = d->server->restConnection()->startLiteClient(handleReply);
    if (d->clientStartHandle <= 0)
        d->setClientStartResult(false);
}

void QnLiteClientController::stopLiteClient()
{
    Q_D(QnLiteClientController);
    if (d->clientState != State::Started)
        return;

    if (!d->server)
        return;

    d->setClientState(State::Stopping);

    ec2::ApiVideowallControlMessageData message;
    message.operation = QnVideoWallControlMessage::Exit;
    message.videowallGuid = d->serverId;

    const auto connection = commonModule()->ec2Connection();
    const auto videowallManager = connection->getVideowallManager(Qn::kSystemAccess);
    videowallManager->sendControlMessage(message, this, []{});
}

QnLiteClientControllerPrivate::QnLiteClientControllerPrivate(QnLiteClientController* parent):
    QObject(parent),
    q_ptr(parent),
    layoutHelper(new QnLiteClientLayoutHelper(parent)),
    startStopTimer(new QTimer(parent))
{
    startStopTimer->setInterval(kStartStopTimeoutMs);
    connect(startStopTimer, &QTimer::timeout,
        this, &QnLiteClientControllerPrivate::at_startStopTimer_timeout);
}

void QnLiteClientControllerPrivate::setClientState(QnLiteClientController::State state)
{
    if (clientState == state)
        return;

    clientState = state;

    switch (state)
    {
        case QnLiteClientController::State::Stopped:
        case QnLiteClientController::State::Started:
            startStopTimer->stop();
            break;
        case QnLiteClientController::State::Stopping:
        case QnLiteClientController::State::Starting:
            startStopTimer->start();
            break;
    }

    Q_Q(QnLiteClientController);
    emit q->clientStateChanged();
}

void QnLiteClientControllerPrivate::setClientStartResult(bool success)
{
    Q_Q(QnLiteClientController);

    clientStartHandle = -1;

    if (success)
    {
        setClientState(QnLiteClientController::State::Started);
    }
    else
    {
        setClientState(QnLiteClientController::State::Stopped);
        emit q->clientStartError();
    }
}

void QnLiteClientControllerPrivate::setClientStopResult(bool success)
{
    Q_Q(QnLiteClientController);

    if (success)
    {
        setClientState(QnLiteClientController::State::Stopped);
    }
    else
    {
        setClientState(QnLiteClientController::State::Started);
        emit q->clientStopError();
    }
}

void QnLiteClientControllerPrivate::updateServerStatus()
{
    const bool online = (server && server->getStatus() == Qn::Online);

    if (serverOnline == online)
        return;

    serverOnline = online;

    Q_Q(QnLiteClientController);
    emit q->serverOnlineChanged();
}

bool QnLiteClientControllerPrivate::initLayout()
{
    if (!layoutHelper->layout() || layoutHelper->layout()->getParentId() != serverId)
        layoutHelper->setLayout(layoutHelper->findLayoutForServer(serverId));

    if (!layoutHelper->layout())
        layoutHelper->setLayout(layoutHelper->createLayoutForServer(serverId));

    return !layoutHelper->layout().isNull();
}

void QnLiteClientControllerPrivate::at_runtimeInfoAdded(const QnPeerRuntimeInfo& data)
{
    if (serverId.isNull())
        return;

    // Assuming a Mobile Client with proper videowallInstanceGuid is a Lite Client.

    if (data.data.peer.peerType != Qn::PT_MobileClient)
        return;

    if (data.data.videoWallInstanceGuid != serverId)
        return;

    bool ok = initLayout();
    setClientStartResult(ok);
}

void QnLiteClientControllerPrivate::at_runtimeInfoRemoved(const QnPeerRuntimeInfo& data)
{
    if (serverId.isNull())
        return;

    if (data.data.peer.peerType != Qn::PT_MobileClient)
        return;

    if (data.data.videoWallInstanceGuid != serverId)
        return;

    setClientStopResult(true);
}

void QnLiteClientControllerPrivate::at_startStopTimer_timeout()
{
    if (clientState == QnLiteClientController::State::Starting)
        setClientStartResult(false);
    else if (clientState == QnLiteClientController::State::Stopping)
        setClientStopResult(false);
}
