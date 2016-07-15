#include <functional>
#include <memory>

#include <business/business_event_rule.h>
#include <nx_ec/dummy_handler.h>
#include <utils/common/synctime.h>
#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>

#include "adam_resource.h"
#include "adam_modbus_io_manager.h"


const QString QnAdamResource::kManufacture(lit("AdvantechADAM"));

QnAdamResource::QnAdamResource()
{
    
}

QnAdamResource::~QnAdamResource()
{
    stopInputPortMonitoringAsync();
}

QString QnAdamResource::getDriverName() const
{
    return kManufacture;
}

CameraDiagnostics::Result QnAdamResource::initInternal()
{
    QnSecurityCamResource::initInternal();

    Qn::CameraCapabilities caps = Qn::NoCapabilities;

    caps |= Qn::RelayInputCapability;
    caps |= Qn::RelayOutputCapability;

    m_ioManager.reset(new QnAdamModbusIOManager(this));

    QnIOPortDataList allPorts = m_ioManager->getInputPortList();
    QnIOPortDataList outputPorts = m_ioManager->getOutputPortList();
    allPorts.insert(allPorts.begin(), outputPorts.begin(), outputPorts.end());

    setIOPorts(allPorts);
    setCameraCapabilities(caps);
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool QnAdamResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
{
    QN_UNUSED(completionHandler);

    auto callback = [this](QString portId, nx_io_managment::IOPortState inputState)
    {
        bool isActive = inputState == nx_io_managment::IOPortState::active;
        emit cameraInput(
            toSharedPointer(),
            portId, 
            isActive,
            qnSyncTime->currentUSecsSinceEpoch());
    };

    m_ioManager->setInputPortStateChangeCallback(callback);
    return m_ioManager->startIOMonitoring();
}

void QnAdamResource::stopInputPortMonitoringAsync()
{
    if (m_ioManager)
        m_ioManager->stopIOMonitoring();
}

bool QnAdamResource::isInputPortMonitored() const
{
    return m_ioManager->isMonitoringInProgress();
}

QnIOPortDataList QnAdamResource::getRelayOutputList() const
{
    return m_ioManager->getOutputPortList();
}

QnIOPortDataList QnAdamResource::getInputPortList() const
{
    return m_ioManager->getInputPortList();
}

bool QnAdamResource::setRelayOutputState(
    const QString& outputId, 
    bool isActive, 
    unsigned int autoResetTimeoutMS )
{

    // TODO: #dmishin implement auto-reset timeout.
    return m_ioManager->setOutputPortState(outputId, isActive);
}
