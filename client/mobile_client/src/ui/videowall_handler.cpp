#include "videowall_handler.h"

#include <mobile_client/mobile_client_message_processor.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/data/api_videowall_data.h>
#include <api/runtime_info_manager.h>
#include <mobile_client/mobile_client_ui_controller.h>

// TODO mike: REMOVE
#define OUTPUT_PREFIX "mobile_client[videowall_handler]: "
#include <nx/utils/debug_utils.h>
#include <nx/utils/singleton.h>

namespace {

#define PARAM_KEY(KEY) const QLatin1String KEY##Key(BOOST_PP_STRINGIZE(KEY));
PARAM_KEY(sequence)
PARAM_KEY(pcUuid)
PARAM_KEY(uuid)
PARAM_KEY(zoomUuid)
PARAM_KEY(resource)
PARAM_KEY(value)
PARAM_KEY(role)
PARAM_KEY(position)
PARAM_KEY(rotation)
PARAM_KEY(speed)
PARAM_KEY(geometry)
PARAM_KEY(zoomRect)
PARAM_KEY(checkedButtons)

} // namespace

QnVideowallHandler::QnVideowallHandler(QObject* parent):
    base_type(parent)
{
    PRINT << "new QnVideowallHandler";
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::videowallControlMessageReceived,
        this, &QnVideowallHandler::at_controlMessageReceived);
}

void QnVideowallHandler::at_controlMessageReceived(
    const ec2::ApiVideowallControlMessageData& apiMessage)
{
    if (apiMessage.instanceGuid !=
        QnRuntimeInfoManager::instance()->localInfo().data.videoWallInstanceGuid)
    {
        PRINT << "RECEIVED VideowallControlMessage for guid " << apiMessage.instanceGuid.toString()
            << "; ignored";
        return;
    }
    PRINT << "RECEIVED VideowallControlMessage, guid " << apiMessage.instanceGuid.toString();

    QnVideoWallControlMessage message;
    fromApiToResource(apiMessage, message);

    QnUuid controllerUuid = QnUuid(message[pcUuidKey]);
    qint64 sequence = message[sequenceKey].toULongLong();

    handleMessage(message, controllerUuid, sequence);
}

// TODO mike: Find a proper place to call QnMobileClientUiController::setLayoutId().

void QnVideowallHandler::handleMessage(
    const QnVideoWallControlMessage& message,
    const QnUuid& controllerUuid,
    qint64 sequence)
{
    Q_UNUSED(controllerUuid);
    Q_UNUSED(sequence);

    switch (static_cast<QnVideoWallControlMessage::Operation>(message.operation))
    {
        case QnVideoWallControlMessage::Exit:
        {
            PRINT << "RECEIVED VideowallControlMessage::Exit";
            QCoreApplication::quit();
            break;
        }

        case QnVideoWallControlMessage::EnterFullscreen:
        {
            PRINT << "RECEIVED VideowallControlMessage::EnterFullscreen";

            // TODO mike: Extract from params.
            QnUuid cameraId;

            NX_ASSERT(m_uiController);
            if (m_uiController)
                m_uiController->openVideoScreen(cameraId);
            break;
        }

        case QnVideoWallControlMessage::ExitFullscreen:
        {
            PRINT << "RECEIVED VideowallControlMessage::ExitFullscreen";
            NX_ASSERT(m_uiController);
            if (m_uiController)
                m_uiController->openResourcesScreen();
            break;
        }

        default:
            break;
    }
}

void QnVideowallHandler::setUiController(QnMobileClientUiController* uiController)
{
    m_uiController = uiController;
}
