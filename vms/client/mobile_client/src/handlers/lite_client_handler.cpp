#include "lite_client_handler.h"

#include <mobile_client/mobile_client_message_processor.h>
#include <mobile_client/mobile_client_ui_controller.h>
#include <client_core/connection_context_aware.h>

#include <api/runtime_info_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <common/common_module.h>
#include <nx/client/mobile/resource/lite_client_layout_helper.h>

using namespace nx::client::mobile;

class QnLiteClientHandlerPrivate: public QObject, public QnConnectionContextAware
{
    QnLiteClientHandler* q_ptr;
    Q_DECLARE_PUBLIC(QnLiteClientHandler)

public:
    QnLiteClientHandlerPrivate(QnLiteClientHandler* parent);

    void at_videowallControlMessageReceived(const nx::vms::api::VideowallControlMessageData& message);
    void at_initialResourcesReceived();
    void at_resourceAdded(const QnResourcePtr& resource);

public:
    QnMobileClientUiController* uiController = nullptr;
    QnUuid videowallGuid;
    QnLayoutResourcePtr layout;
};

QnLiteClientHandler::QnLiteClientHandler(QObject* parent):
    base_type(parent),
    d_ptr(new QnLiteClientHandlerPrivate(this))
{
}

QnLiteClientHandler::~QnLiteClientHandler()
{
}

void QnLiteClientHandler::setUiController(QnMobileClientUiController* controller)
{
    Q_D(QnLiteClientHandler);
    d->uiController = controller;
    if (d->uiController && d->layout)
        d->uiController->setLayoutId(d->layout->getId().toString());
}

QnLiteClientHandlerPrivate::QnLiteClientHandlerPrivate(QnLiteClientHandler* parent):
    QObject(parent),
    q_ptr(parent),
    videowallGuid(runtimeInfoManager()->localInfo().data.videoWallInstanceGuid)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::videowallControlMessageReceived,
        this, &QnLiteClientHandlerPrivate::at_videowallControlMessageReceived);
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived,
            this, &QnLiteClientHandlerPrivate::at_initialResourcesReceived);
}

void QnLiteClientHandlerPrivate::at_videowallControlMessageReceived(
    const nx::vms::api::VideowallControlMessageData& message)
{
    if (message.videowallGuid != videowallGuid)
        return;

    switch (message.operation)
    {
        case QnVideoWallControlMessage::Exit:
            if (uiController)
                uiController->disconnectFromSystem();
            break;

        default:
            break;
    }
}

void QnLiteClientHandlerPrivate::at_initialResourcesReceived()
{
    resource::LiteClientLayoutHelper helper;

    layout = helper.findLayoutForServer(videowallGuid);
    if (!layout)
    {
        layout = helper.createLayoutForServer(videowallGuid);

        if (!layout)
            return;
    }

    if (uiController)
        uiController->setLayoutId(layout->getId().toString());
}
