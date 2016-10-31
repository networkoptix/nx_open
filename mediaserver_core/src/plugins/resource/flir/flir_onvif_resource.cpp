#include <QtWebSockets/QWebSocket>

#include "flir_onvif_resource.h"

#include <utils/common/synctime.h>

#ifdef ENABLE_ONVIF

QnFlirOnvifResource::QnFlirOnvifResource()
{
}

CameraDiagnostics::Result QnFlirOnvifResource::initInternal()
{
    auto result = QnPlOnvifResource::initInternal();

    if (result != CameraDiagnostics::NoErrorResult())
        return result;

    m_ioManager.reset(new FlirWsIOManager(this));

    Qn::CameraCapabilities caps = Qn::NoCapabilities;
    QnIOPortDataList allPorts = getInputPortList();
    QnIOPortDataList outputPorts = getRelayOutputList();

    if (!allPorts.empty())
        caps |= Qn::RelayInputCapability;

    if (!outputPorts.empty())
        caps |= Qn::RelayOutputCapability;
    
    allPorts.insert(allPorts.begin(), outputPorts.begin(), outputPorts.end());

    setIOPorts(allPorts);
    setCameraCapabilities(caps);

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool QnFlirOnvifResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
{
    if (m_ioManager->isMonitoringInProgress())
        return false;

    m_ioManager->setInputPortStateChangeCallback(
        [this](QString portId, nx_io_managment::IOPortState portState)
        {
            emit cameraInput(
                toSharedPointer(this),
                portId,
                nx_io_managment::isActiveIOPortState(portState),
                qnSyncTime->currentUSecsSinceEpoch());
        });

    m_ioManager->setNetworkIssueCallback(
        [](QString reason, bool isFatal)
        {
            qDebug () << "Flir, network issue occured during io monitoring";
        });

    m_ioManager->startIOMonitoring();
    return true;
}

void QnFlirOnvifResource::stopInputPortMonitoringAsync()
{
    if (m_ioManager->isMonitoringInProgress())
        return; 

    m_ioManager->stopIOMonitoring();
}

QnIOPortDataList QnFlirOnvifResource::getInputPortList() const
{
    if (m_ioManager)
        return m_ioManager->getInputPortList();

    return QnIOPortDataList();
}


bool QnFlirOnvifResource::setRelayOutputState(
    const QString& outputID,
    bool isActive,
    unsigned int autoResetTimeoutMS)
{
    //Not sure if it's right
    return QnPlOnvifResource::setRelayOutputState(
        outputID,
        isActive,
        autoResetTimeoutMS);
}

QnIOPortDataList QnFlirOnvifResource::getRelayOutputList() const
{
    if (m_ioManager)
        return m_ioManager->getOutputPortList();

    return QnIOPortDataList();
}

#endif // ENABLE_ONVIF
