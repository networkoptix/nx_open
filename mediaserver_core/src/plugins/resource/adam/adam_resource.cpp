#ifdef ENABLE_ADVANTECH

#include <functional>
#include <memory>

#include <business/business_event_rule.h>
#include <nx_ec/dummy_handler.h>
#include <utils/common/synctime.h>
#include <utils/serialization/json.h>
#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/common/model_functions.h>

#include "adam_resource.h"
#include "adam_modbus_io_manager.h"


const QString QnAdamResource::kManufacture(lit("AdvantechADAM"));

QnAdamResource::QnAdamResource()
{
    connect(
        this, &QnResource::propertyChanged, 
        this, &QnAdamResource::at_propertyChanged, 
        Qt::DirectConnection );
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

    QnIOPortDataList allPorts = getInputPortList();
    QnIOPortDataList outputPorts = getRelayOutputList();
    allPorts.insert(allPorts.begin(), outputPorts.begin(), outputPorts.end());

    setPortDefaultStates();
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
        bool isDefaultPortStateActive = 
            nx_io_managment::isActiveIOPortState(
                m_ioManager->getPortDefaultState(portId));

        bool isActive = nx_io_managment::isActiveIOPortState(inputState);

        emit cameraInput(
            toSharedPointer(),
            portId, 
            isActive != isDefaultPortStateActive,
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

QnIOPortDataList QnAdamResource::mergeIOPortData(
    const QnIOPortDataList& deviceIO, 
    const QnIOPortDataList& savedIO) const
{
    QnIOPortDataList resultIO = deviceIO;
    for (auto& result : resultIO)
    {
        for(const auto& saved : savedIO)
        {
            if (result.id == saved.id)
                result = saved;
                break;
        }
    }

    return resultIO;
}

void QnAdamResource::setPortDefaultStates()
{
    auto ports = QJson::deserialized<QnIOPortDataList>(
        getProperty(Qn::IO_SETTINGS_PARAM_NAME).toUtf8());

    for (const auto& port: ports)
    {
        auto portDefaultState = port.portType == Qn::PT_Input ? 
            port.iDefaultState : port.oDefaultState;

        m_ioManager->setPortDefaultState(
            port.id, 
            nx_io_managment::fromDefaultPortState(portDefaultState));
    }
}

QnIOPortDataList QnAdamResource::getRelayOutputList() const
{
    if (m_ioManager)
    {
        auto deviceOutputs = m_ioManager->getOutputPortList();
        auto savedIO = QJson::deserialized<QnIOPortDataList>(
            getProperty(Qn::IO_SETTINGS_PARAM_NAME).toUtf8());

        QnIOPortDataList savedOutputs;

        for (const auto& ioPort: savedIO)
        {
            if (ioPort.portType == Qn::PT_Output)
                savedOutputs.push_back(ioPort);
        }

        return mergeIOPortData(deviceOutputs, savedOutputs);
    }

    return QnIOPortDataList();
}

QnIOPortDataList QnAdamResource::getInputPortList() const
{
    if (m_ioManager)
    {
        auto deviceInputs = m_ioManager->getInputPortList();
        auto savedIO = QJson::deserialized<QnIOPortDataList>(
            getProperty(Qn::IO_SETTINGS_PARAM_NAME).toUtf8());

        QnIOPortDataList savedInputs;

        for (const auto& ioPort: savedIO)
        {
            if (ioPort.portType == Qn::PT_Input)
                savedInputs.push_back(ioPort);
        }

        return mergeIOPortData(deviceInputs, savedInputs);
    }

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

    if (!isActive)
    {
        for (auto it = m_autoResetTimers.begin(); it != m_autoResetTimers.end();)
        {
            auto timerId = it->first;
            auto portTimerEntry = it->second;
            if (it->second.portId == outputId)
            {
                TimerManager::instance()->deleteTimer(timerId);
                it = m_autoResetTimers.erase(it);
                break;
            }
            else
            {
                ++it;
            }
        }
    }

    if (isActive && autoResetTimeoutMs)
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
    if (key == Qn::IO_SETTINGS_PARAM_NAME && res && !res->hasFlags(Qn::foreigner))
    {
        auto ports = QJson::deserialized<QnIOPortDataList>(getProperty(Qn::IO_SETTINGS_PARAM_NAME).toUtf8());
        setPortDefaultStates();
        setIOPorts(ports);
        saveParams();
    }
}

#endif //< ENABLE_ADVANTECH
