#ifdef ENABLE_ADVANTECH

#include <functional>
#include <memory>

#include <nx_ec/dummy_handler.h>
#include <utils/common/synctime.h>
#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/events/network_issue_event.h>
#include <modbus/modbus_client.h>

#include "adam_resource.h"
#include "adam_modbus_io_manager.h"
#include <nx/utils/timer_manager.h>

const QString QnAdamResource::kManufacture(lit("AdvantechADAM"));

QnAdamResource::QnAdamResource(QnMediaServerModule* serverModule):
    nx::vms::server::resource::Camera(serverModule)
{
    Qn::directConnect(
        this, &QnResource::propertyChanged,
        this, &QnAdamResource::at_propertyChanged);
}

QnAdamResource::~QnAdamResource()
{
    directDisconnectAll();
    stopInputPortStatesMonitoring();
    if (m_ioManager)
        m_ioManager->terminate();
}

QString QnAdamResource::getDriverName() const
{
    return kManufacture;
}

nx::vms::server::resource::StreamCapabilityMap QnAdamResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex streamIndex)
{
    // TODO: implement me
    return nx::vms::server::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result QnAdamResource::initializeCameraDriver()
{
    QUrl url(getUrl());
    auto host  = url.host();
    auto port = url.port(nx::modbus::kDefaultModbusPort);

    nx::network::SocketAddress endpoint(host, port);

    nx::modbus::QnModbusClient testClient(endpoint);

    int triesLeft = 3;
    bool status = false;

    while (!status && triesLeft--)
        testClient.readCoils(0, 8, &status);

    if (!status)
    {
        return CameraDiagnostics::RequestFailedResult(
            lit("Test request failed"),
            lit("couldn't get valid response from device"));
    }

    m_ioManager.reset(new QnAdamModbusIOManager(this));

    auto ports = m_ioManager->getInputPortList();
    auto outputs = m_ioManager->getOutputPortList();
    ports.insert(ports.begin(), ports.begin(), ports.end());

    m_portTypes.clear();
    for (const auto& port: ports)
        m_portTypes[port.id] = port.portType;

    setPortDefaultStates();
    setIoPortDescriptions(std::move(ports), /*needMerge*/ true);

    setFlags(flags() | Qn::io_module);
    setProperty(ResourcePropertyKey::kIoConfigCapability, 1);

    saveProperties();

    return CameraDiagnostics::NoErrorResult();
}

void QnAdamResource::startInputPortStatesMonitoring()
{
    if (!m_ioManager)
        return;

    auto callback = [this](QString portId, nx_io_managment::IOPortState inputState)
    {
        bool isDefaultPortStateActive =
            nx_io_managment::isActiveIOPortState(
                m_ioManager->getPortDefaultState(portId));

        bool isActive = nx_io_managment::isActiveIOPortState(inputState);

        const auto ioPortType = portType(portId);

        if (ioPortType == Qn::PT_Input)
        {
            emit inputPortStateChanged(
                toSharedPointer(),
                portId,
                isActive != isDefaultPortStateActive,
                qnSyncTime->currentUSecsSinceEpoch());
        }
        else if (ioPortType == Qn::PT_Output)
        {
            emit outputPortStateChanged(
                toSharedPointer(),
                portId,
                isActive != isDefaultPortStateActive,
                qnSyncTime->currentUSecsSinceEpoch());
        }
    };

    auto networkIssueHandler = [this](QString reason, bool isFatal)
    {
        emit networkIssue(
            toSharedPointer(this),
            qnSyncTime->currentUSecsSinceEpoch(),
            nx::vms::api::EventReason::networkNoResponseFromDevice,
            QString());

        if (isFatal)
            setStatus(Qn::Offline);
    };

    m_ioManager->setInputPortStateChangeCallback(callback);
    m_ioManager->setNetworkIssueCallback(networkIssueHandler);

    m_ioManager->startIOMonitoring();
}

void QnAdamResource::stopInputPortStatesMonitoring()
{
    if (m_ioManager)
        m_ioManager->stopIOMonitoring();
}

void QnAdamResource::setPortDefaultStates()
{
    if (!m_ioManager)
        return;

    auto ports = QJson::deserialized<QnIOPortDataList>(
        getProperty(ResourcePropertyKey::kIoSettings).toUtf8());

    for (const auto& port: ports)
    {
        auto portDefaultState = port.portType == Qn::PT_Input ?
            port.iDefaultState : port.oDefaultState;

        m_ioManager->setPortDefaultState(
            port.id,
            nx_io_managment::fromDefaultPortState(portDefaultState));
    }
}

QnIOStateDataList QnAdamResource::ioPortStates() const
{
    if (m_ioManager)
        return m_ioManager->getPortStates();

    return QnIOStateDataList();
}

bool QnAdamResource::setOutputPortState(
    const QString& outputId,
    bool isActive,
    unsigned int autoResetTimeoutMs )
{
    QnMutexLocker lock(&m_mutex);

    if (!m_ioManager)
        return false;

    if (!isActive)
    {
        for (auto it = m_autoResetTimers.begin(); it != m_autoResetTimers.end(); ++it)
        {
            auto timerId = it->first;
            auto portTimerEntry = it->second;
            if (it->second.portId == outputId)
            {
                nx::utils::TimerManager::instance()->deleteTimer(timerId);
                it = m_autoResetTimers.erase(it);
                break;
            }
        }
    }

    if (isActive && autoResetTimeoutMs)
    {
        auto autoResetTimer = nx::utils::TimerManager::instance()->addTimer(
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
            std::chrono::milliseconds(autoResetTimeoutMs));

        PortTimerEntry portTimerEntry;
        portTimerEntry.portId = outputId;
        portTimerEntry.state = !isActive;
        m_autoResetTimers[autoResetTimer] = portTimerEntry;
    }

    return m_ioManager->setOutputPortState(outputId, isActive);
}

void QnAdamResource::at_propertyChanged(const QnResourcePtr& res, const QString& key)
{
    if (key == ResourcePropertyKey::kIoSettings && res && !res->hasFlags(Qn::foreigner))
    {
        auto ports = ioPortDescriptions();
        setPortDefaultStates();
        setIoPortDescriptions(std::move(ports), /*needMerge*/ true);
        savePropertiesAsync();
    }
}

void QnAdamResource::setIframeDistance(int /*frames*/, int /*timeMs*/)
{
}

Qn::IOPortType QnAdamResource::portType(const QString& portId) const
{
    const auto itr = m_portTypes.find(portId);
    if (itr == m_portTypes.cend())
        return Qn::PT_Unknown;

    return itr->second;
}

#endif //< ENABLE_ADVANTECH
