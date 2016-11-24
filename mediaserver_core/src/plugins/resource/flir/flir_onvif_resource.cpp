#include "flir_onvif_resource.h"
#include "flir_io_executor.h"

#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

#ifdef ENABLE_ONVIF

using namespace nx::plugins;

flir::OnvifResource::OnvifResource():
    m_ioManager(nullptr)
{
}

flir::OnvifResource::~OnvifResource()
{
    stopInputPortMonitoringAsync();

    if (m_ioManager)
    {
        QMetaObject::invokeMethod(
            m_ioManager,
            "deleteLater",
            Qt::QueuedConnection);
    }
}

CameraDiagnostics::Result flir::OnvifResource::initInternal()
{
    auto result = QnPlOnvifResource::initInternal();

    if (result != CameraDiagnostics::NoErrorResult())
        return result;

    if (!m_ioManager)
    {
        m_ioManager = new flir::nexus::WebSocketIoManager(dynamic_cast<QnVirtualCameraResource*>(this));
        m_ioManager->moveToThread(flir::IoExecutor::instance()->getThread());
    }

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

bool flir::OnvifResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
{
    if (!m_ioManager)
        return false;

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
        [this](QString reason, bool /*isFatal*/)
        {
            NX_LOG(
                lm("Flir Onvif resource, %1 (%2), netowk issue detected. Reason: %3")
                    .arg(getModel())
                    .arg(getUrl())
                    .arg(reason), 
                cl_logWARNING);
        });

    m_ioManager->startIOMonitoring();
    return true;
}

void flir::OnvifResource::stopInputPortMonitoringAsync()
{
    if (!m_ioManager)
        return;

    if (!m_ioManager->isMonitoringInProgress())
        return; 

    m_ioManager->stopIOMonitoring();
}

QnIOPortDataList flir::OnvifResource::getInputPortList() const
{
    if (m_ioManager)
        return m_ioManager->getInputPortList();

    return QnIOPortDataList();
}

bool flir::OnvifResource::setRelayOutputState(
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

QnIOPortDataList flir::OnvifResource::getRelayOutputList() const
{
    if (!m_ioManager)
        return QnIOPortDataList();

    auto onvifPorts = QnPlOnvifResource::getRelayOutputList();
    auto nexusPorts = m_ioManager->getOutputPortList();

    onvifPorts.insert(onvifPorts.end(), nexusPorts.begin(), nexusPorts.end());

    return onvifPorts;
}

#endif // ENABLE_ONVIF
