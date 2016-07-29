#include "lite_client_controller.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>
#include <api/runtime_info_manager.h>
#include <api/server_rest_connection.h>
#include <utils/common/id.h>

class QnLiteClientControllerPrivate: public QObject
{
    Q_DECLARE_PUBLIC(QnLiteClientController)
    QnLiteClientController* q_ptr;

public:
    QnMediaServerResourcePtr server;
    QnUuid serverId;
    bool clientOnline = false;
    QnLayoutResourcePtr layout;
    rest::Handle clientStartHandle = -1;

public:
    QnLiteClientControllerPrivate(QnLiteClientController* parent);

    void setClientOnline(bool online);

    static QnLayoutResourcePtr createLayout();

    void at_runtimeInfoAdded(const QnPeerRuntimeInfo& data);
    void at_runtimeInfoRemoved(const QnPeerRuntimeInfo& data);
    void at_runtimeInfoChanged(const QnPeerRuntimeInfo& data);
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

    d->server = qnResPool->getResourceById<QnMediaServerResource>(id);
    emit serverIdChanged();

    if (d->server)
    {
        d->serverId = d->server->getId();
        connect(qnRuntimeInfoManager, &QnRuntimeInfoManager::runtimeInfoAdded,
            d, &QnLiteClientControllerPrivate::at_runtimeInfoAdded);
        connect(qnRuntimeInfoManager, &QnRuntimeInfoManager::runtimeInfoRemoved,
                d, &QnLiteClientControllerPrivate::at_runtimeInfoRemoved);
    }
    else
    {
        disconnect(qnRuntimeInfoManager, nullptr, this, nullptr);
    }
}

bool QnLiteClientController::clientOnline() const
{
    Q_D(const QnLiteClientController);
    return d->clientOnline;
}

QnLayoutProperties::DisplayMode QnLiteClientController::displayMode() const
{
    Q_D(const QnLiteClientController);

    if (!d->layout)
        return QnLayoutProperties::DisplayMode::SingleCamera;

    return static_cast<QnLayoutProperties::DisplayMode>(
        d->layout->getProperty(QnLayoutProperties::kDisplayMode).toInt());
}

void QnLiteClientController::setDisplayMode(QnLayoutProperties::DisplayMode displayMode)
{
    Q_D(QnLiteClientController);

    if (!d->layout)
        return;

    d->layout->setProperty(
        QnLayoutProperties::kDisplayMode, QString::number(static_cast<int>(displayMode)));

    propertyDictionary->saveParams(d->layout->getId());
}

QString QnLiteClientController::singleCameraId() const
{
    Q_D(const QnLiteClientController);

    if (!d->layout)
        return QString();

    return d->layout->getProperty(QnLayoutProperties::kSingleCamera);
}

void QnLiteClientController::setSingleCameraId(const QString& cameraId)
{
    Q_D(QnLiteClientController);

    if (!d->layout)
        return;

    d->layout->setProperty(QnLayoutProperties::kSingleCamera, cameraId);

    propertyDictionary->saveParams(d->layout->getId());
}

QString QnLiteClientController::cameraIdOnCell(int x, int y) const
{
    return QString();
}

void QnLiteClientController::setCameraIdOnCell(const QString& cameraId, int x, int y)
{

}

void QnLiteClientController::startLiteClient()
{
    Q_D(QnLiteClientController);

    if (d->clientOnline)
        return;

    if (!d->server)
        return;

    const auto resources = qnResPool->getResourcesByParentId(d->serverId);
    const auto it = std::find_if(resources.begin(), resources.end(),
        [id = d->serverId](const QnResourcePtr& resource)
        {
            const auto layout = resource.dynamicCast<QnLayoutResource>();
            return !layout.isNull();
        });

    if (it != resources.end())
        d->layout = *it;
    else
        d->layout = d->createLayout();

    if (!d->layout)
    {
        emit clientStartError();
        return;
    }

    auto handleReply = [this, d](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            Q_UNUSED(result);

            if (d->clientStartHandle != handle)
                return;

            if (!success)
                emit clientStartError();
        };

    d->server->restConnection()->startLiteClient(handleReply);
}

void QnLiteClientController::stopLiteClient()
{
    Q_D(QnLiteClientController);
    if (!d->clientOnline)
        return;

    if (!d->server)
        return;
}

QnLiteClientControllerPrivate::QnLiteClientControllerPrivate(QnLiteClientController* parent):
    QObject(parent),
    q_ptr(parent)
{
}

void QnLiteClientControllerPrivate::setClientOnline(bool online)
{
    if (clientOnline == online)
        return;

    clientOnline = online;

    Q_Q(QnLiteClientController);
    emit q->clientOnlineChanged();
}

QnLayoutResourcePtr QnLiteClientControllerPrivate::createLayout()
{
    auto layout = QnLayoutResourcePtr(new QnLayoutResource());
    layout->setId(QnUuid::createUuid());

    layout->setName(server->getName());
    layout->addFlags(Qn::local); // TODO: #Elric #EC2

    qnResPool->markLayoutAutoGenerated(layout);
    qnResPool->addResource(layout);
    qnResourcesChangesManager->saveLayout(d->layout, [](const QnLayoutResourcePtr&){});

    return layout;
}

void QnLiteClientControllerPrivate::at_runtimeInfoAdded(const QnPeerRuntimeInfo& data)
{
    if (serverId.isNull())
        return;

    if (data.data.peer.peerType != Qn::PT_LiteClient)
        return;

    if (data.data.videoWallInstanceGuid != serverId)
        return;

    setClientOnline(true);
}

void QnLiteClientControllerPrivate::at_runtimeInfoRemoved(const QnPeerRuntimeInfo& data)
{
    if (serverId.isNull())
        return;

    if (data.data.peer.peerType != Qn::PT_LiteClient)
        return;

    if (data.data.videoWallInstanceGuid != serverId)
        return;

    setClientOnline(false);
}
