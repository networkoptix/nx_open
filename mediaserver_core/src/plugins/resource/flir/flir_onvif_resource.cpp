#include "flir_onvif_resource.h"
#include "flir_io_executor.h"

#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

#ifdef ENABLE_ONVIF

QnFlirOnvifResource::QnFlirOnvifResource()
{
}

QnFlirOnvifResource::~QnFlirOnvifResource()
{
    stopInputPortMonitoringAsync();
    m_ioManager->terminate();
    if (m_ioManager)
    {
        QMetaObject::invokeMethod(
            m_ioManager,
            "deleteLater",
            Qt::QueuedConnection);
    }
}

CameraDiagnostics::Result QnFlirOnvifResource::initInternal()
{
    auto result = QnPlOnvifResource::initInternal();

    if (result != CameraDiagnostics::NoErrorResult())
        return result;

    if (!m_ioManager)
    {
        m_ioManager = new FlirWsIOManager(this);
        m_ioManager->moveToThread(FlirIOExecutor::instance()->getThread());
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

bool QnFlirOnvifResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
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

void QnFlirOnvifResource::stopInputPortMonitoringAsync()
{
    if (!m_ioManager)
        return;

    if (!m_ioManager->isMonitoringInProgress())
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
    return QnPlOnvifResource::getRelayOutputList();
}

#endif // ENABLE_ONVIF
