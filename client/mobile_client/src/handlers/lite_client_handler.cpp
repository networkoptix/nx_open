#include "lite_client_handler.h"

#include <api/runtime_info_manager.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <mobile_client/mobile_client_message_processor.h>
#include <mobile_client/mobile_client_ui_controller.h>

class QnLiteClientHandlerPrivate: public QObject
{
    QnLiteClientHandler* q_ptr;
    Q_DECLARE_PUBLIC(QnLiteClientHandler)

public:
    QnLiteClientHandlerPrivate(QnLiteClientHandler* parent);

    void at_videowallControlMessageReceived(const ec2::ApiVideowallControlMessageData& message);

public:
    QnMobileClientUiController* uiController = nullptr;
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
}

QnLiteClientHandlerPrivate::QnLiteClientHandlerPrivate(QnLiteClientHandler* parent):
    QObject(parent),
    q_ptr(parent)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::videowallControlMessageReceived,
        this, &QnLiteClientHandlerPrivate::at_videowallControlMessageReceived);
}

void QnLiteClientHandlerPrivate::at_videowallControlMessageReceived(
    const ec2::ApiVideowallControlMessageData& message)
{
    if (message.videowallGuid !=
        QnRuntimeInfoManager::instance()->localInfo().data.videoWallInstanceGuid)
    {
        return;
    }

    switch (message.operation)
    {
        case QnVideoWallControlMessage::Exit:
            qApp->quit();
            break;

        default:
            break;
    }
}
