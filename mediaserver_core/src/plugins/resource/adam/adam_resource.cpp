#ifdef ENABLE_ADVANTECH

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
    if (m_ioManager)
        m_ioManager->terminate();
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

    setFlags(flags() | Qn::io_module);
    setProperty(Qn::VIDEO_DISABLED_PARAM_NAME, 1);
    setProperty(Qn::IO_CONFIG_PARAM_NAME, 1);

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool QnAdamResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
{
    QN_UNUSED(completionHandler);

    auto callback = [this](QString portId, nx_io_managment::IOPortState inputState)
    {
        bool isActive = nx_io_managment::isActiveIOPortState(inputState);
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
    if (m_ioManager)
        return m_ioManager->isMonitoringInProgress();

    return false;
}

QnIOPortDataList QnAdamResource::getRelayOutputList() const
{
    if (m_ioManager)
        return m_ioManager->getOutputPortList();

    return QnIOPortDataList();
}

QnIOPortDataList QnAdamResource::getInputPortList() const
{
    if (m_ioManager)
        return m_ioManager->getInputPortList();

    return QnIOPortDataList();
}

QnIOStateDataList QnAdamResource::ioStates() const 
{
    if (m_ioManager)
        return m_ioManager->getPortStates();

    return QnIOStateDataList();
}

bool QnAdamResource::setRelayOutputState(
    const QString& outputId, 
    bool isActive, 
    unsigned int autoResetTimeoutMs )
{
    QnMutexLocker lock(&m_mutex);

    if (!m_ioManager)
        return false;

    for (auto it = m_autoResetTimers.begin(); it != m_autoResetTimers.end();)
    {
        if (it->second.portId == outputId)
            it = m_autoResetTimers.erase(it);
        else
            ++it;
    }

    if (autoResetTimeoutMs)
    {
        auto autoResetTimer = TimerManager::instance()->addTimer(
            [this](quint64  timerId)
            {
                QnMutexLocker lock(&m_mutex);
                if (m_autoResetTimers.count(timerId))
                {
                    auto timerEntry = m_autoResetTimers[timerId];
                    m_ioManager->setOutputPortState(
                        timerEntry.portId, 
                        timerEntry.state);
                }
                m_autoResetTimers.erase(timerId);
            },
            autoResetTimeoutMs);

        PortTimerEntry portTimerEntry;
        portTimerEntry.portId = outputId;
        portTimerEntry.state = !isActive;
        m_autoResetTimers[autoResetTimer] = portTimerEntry;
    }

    return m_ioManager->setOutputPortState(outputId, isActive);
}

void QnAdamResource::at_propertyChanged(const QnResourcePtr &res, const QString &key)
{
    qDebug() << "Resource property has changed:" << res->getName() << key;

    if (key == Qn::IO_SETTINGS_PARAM_NAME && res && !res->hasFlags(Qn::foreigner))
        qDebug() << getProperty(Qn::IO_SETTINGS_PARAM_NAME);
}

#endif //< ENABLE_ADVANTECH
