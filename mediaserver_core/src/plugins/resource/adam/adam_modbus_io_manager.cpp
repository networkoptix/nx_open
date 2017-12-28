#ifdef ENABLE_ADVANTECH

#include <core/resource/security_cam_resource.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

#include "adam_modbus_io_manager.h"
#include <nx/utils/timer_manager.h>
#include <common/static_common_module.h>

using namespace nx_io_managment;

namespace
{
    const unsigned int kInputPollingIntervalMs = 300;
    const unsigned int kDefaultFirstInputCoilAddress = 0;
    const unsigned int kDefaultFirstOutputCoilAddress = 16;

    const QString kAdamStartInputCoilParamName("adamStartInputCoil");
    const QString kAdamStartOutputCoilParamName("adamStartOutputCoil");
    const QString kAdamInputCountParamName("adamInputCount");
    const QString kAdamOutputCountParamName("adamOutputCount");

    const int kDebounceIterationCount = 4;
    const quint8 kMaxNetworkFaultsNumber = 4;

    const IOPortState kPortDefaultState = IOPortState::nonActive;
}

QnAdamModbusIOManager::QnAdamModbusIOManager(QnResource* resource) :
    m_resource(resource),
    m_monitoringIsInProgress(false),
    m_ioPortInfoFetched(false),
    m_networkFaultsCounter(0)
{
    initializeIO();

    Qn::directConnect(
        &m_client, &nx::modbus::QnModbusAsyncClient::done,
        this, &QnAdamModbusIOManager::routeMonitoringFlow);

    Qn::directConnect(
        &m_client, &nx::modbus::QnModbusAsyncClient::error,
        this, &QnAdamModbusIOManager::handleMonitoringError);
}

QnAdamModbusIOManager::~QnAdamModbusIOManager()
{
    terminate();
    directDisconnectAll();
}

bool QnAdamModbusIOManager::startIOMonitoring()
{
    QnMutexLocker lock(&m_mutex);

    if (m_monitoringIsInProgress)
        return true;

    m_ioPortInfoFetched = initializeIO();

    if (!m_ioPortInfoFetched)
        return false;

    auto securityResource = dynamic_cast<QnSecurityCamResource*>(
        const_cast<QnResource*>(m_resource));

    if (!securityResource)
        return false;

    bool shouldNotStartMonitoring =
        securityResource->hasFlags(Qn::foreigner)
        || !securityResource->hasCameraCapabilities(Qn::RelayInputCapability);

    if (shouldNotStartMonitoring)
        return false;

    m_debouncedValues.clear();

    m_monitoringIsInProgress = true;

    QUrl url(m_resource->getUrl());
    auto host = url.host();
    auto port = url.port(nx::modbus::kDefaultModbusPort);

    nx::network::SocketAddress endpoint(host, port);

    m_client.setEndpoint(endpoint);
    m_outputClient.setEndpoint(endpoint);

    fetchAllPortStatesUnsafe();

    return true;
}

void QnAdamModbusIOManager::stopIOMonitoring()
{
    m_monitoringIsInProgress = false;

    nx::utils::TimerId timerId = 0;

    {
        QnMutexLocker lock(&m_mutex);
        timerId = m_inputMonitorTimerId;
    }

    if (timerId)
        nx::utils::TimerManager::instance()->joinAndDeleteTimer(timerId);
}

bool QnAdamModbusIOManager::setOutputPortState(const QString& outputId, bool isActive)
{
    bool success = true;

    auto coil = getPortCoil(outputId, success);

    auto defaultPortStateIsActive =
        nx_io_managment::isActiveIOPortState(getPortDefaultState(outputId));

    nx::modbus::ModbusResponse response;
    bool status = false;
    int triesLeft = 3;

    while (!status && triesLeft--)
    {
        response = m_outputClient.writeSingleCoil(
            coil,
            isActive != defaultPortStateIsActive,
            &status);

        if (!status)
            qDebug() << "Failed to set port state" << outputId << "tries left:" << triesLeft;
    }

    if (!status && m_networkIssueCallback)
    {
        m_networkIssueCallback(
            lit("Couldn't set port %1 to %2 state")
            .arg(outputId)
            .arg(isActive ? lit("active") : lit("non-active")),
            false);
    }

    if (response.isException() || !status)
        return false;

    setDebounceForPort(outputId, isActive);

    return true;
}

bool QnAdamModbusIOManager::isMonitoringInProgress() const
{
    return m_monitoringIsInProgress;
}

QnIOPortDataList QnAdamModbusIOManager::getInputPortList() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inputs;
}

QnIOPortDataList QnAdamModbusIOManager::getOutputPortList() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputs;
}

QnIOStateDataList QnAdamModbusIOManager::getPortStates() const
{
    return getDebouncedStates();
}

nx_io_managment::IOPortState QnAdamModbusIOManager::getPortDefaultState(const QString& portId) const
{
    QnMutexLocker lock(&m_mutex);
    return getPortDefaultStateUnsafe(portId);
}

void QnAdamModbusIOManager::setPortDefaultState(const QString& portId, nx_io_managment::IOPortState state)
{
    QnMutexLocker lock(&m_mutex);
    m_defaultPortStates[portId] = state;
}

void QnAdamModbusIOManager::setInputPortStateChangeCallback(InputStateChangeCallback callback)
{
    m_inputStateChangedCallback = callback;
}

void QnAdamModbusIOManager::setNetworkIssueCallback(NetworkIssueCallback callback)
{
    m_networkIssueCallback = callback;
}

void QnAdamModbusIOManager::terminate()
{
    stopIOMonitoring();
    m_client.pleaseStopSync();
}

quint32 QnAdamModbusIOManager::getPortCoil(const QString& ioPortId, bool& success) const
{
    auto split = ioPortId.split("_");

    if (split.size() != 2)
    {
        success = false;
        return 0;
    }

    return split[1].toLong(&success);
}

bool QnAdamModbusIOManager::initializeIO()
{
    if (!m_resource)
        return false;

    if (m_ioPortInfoFetched)
        return true;

    auto securityResource = dynamic_cast<QnSecurityCamResource*>(m_resource);

    auto resourceData = qnStaticCommon->dataPool()->data(
        securityResource->getVendor(),
        securityResource->getModel());

    auto startInputCoil = resourceData.value<quint16>(kAdamStartInputCoilParamName);
    auto startOutputCoil = resourceData.value<quint16>(kAdamStartOutputCoilParamName);
    auto inputCount = resourceData.value<quint16>(kAdamInputCountParamName);
    auto outputCount = resourceData.value<quint16>(kAdamOutputCountParamName);

    for (auto i = startInputCoil; i < startInputCoil + inputCount; ++i)
    {
        QnIOPortData port;
        port.id = lit("coil_%1").arg(i);
        port.inputName = lit("DI%1").arg(i);
        port.portType = Qn::PT_Input;
        port.supportedPortTypes = Qn::PT_Input;
        m_inputs.push_back(port);

        m_ioStates.emplace_back(port.id, false, qnSyncTime->currentMSecsSinceEpoch());
    }


    for (auto i = startOutputCoil; i < startOutputCoil + outputCount; ++i)
    {
        QnIOPortData port;
        port.id = lit("coil_%1").arg(i);
        port.outputName = lit("DO%1").arg(i - startOutputCoil);
        port.portType = Qn::PT_Output;
        port.supportedPortTypes = Qn::PT_Output;
        m_outputs.push_back(port);

        m_ioStates.emplace_back(port.id, false, qnSyncTime->currentMSecsSinceEpoch());
    }

    m_ioPortInfoFetched = true;

    return true;
}

void QnAdamModbusIOManager::fetchAllPortStatesUnsafe()
{
    if (m_inputs.empty() || m_outputs.empty())
        return;

    bool status = true;
    auto startCoil = getPortCoil(m_inputs[0].id, status);
    auto lastCoil = getPortCoil(m_outputs[m_outputs.size() - 1].id, status);

    quint16 transactionId = 0;
    if (status)
        m_client.readCoilsAsync(startCoil, lastCoil - startCoil + 1, &transactionId);
}

void QnAdamModbusIOManager::fetchAllPortStates()
{
    QnMutexLocker lock(&m_mutex);
    fetchAllPortStatesUnsafe();
}

void QnAdamModbusIOManager::processAllPortStatesResponse(const nx::modbus::ModbusMessage& response)
{
    QnMutexLocker lock(&m_mutex);

    if (!m_monitoringIsInProgress)
        return;

    if (response.data.isEmpty())
        return;

    if (response.isException())
    {
        qDebug()
            << lit("QnAdamModbusIOManager::processAllPortStatesResponse(), Exception has occured %1")
            .arg(m_client.getLastErrorString());
        return;
    }

    bool status = true;
    auto inputCount = m_inputs.size();
    auto outputCount = m_outputs.size();

    if (!inputCount || !outputCount)
        return;

    auto startInputCoilBit = getPortCoil(m_inputs[0].id, status);
    auto startOutputCoilBit = getPortCoil(m_outputs[0].id, status);

    auto fetchedPortStates = response.data.mid(1);

    if (!status)
        return;

    auto endInputCoilBit = startInputCoilBit + inputCount;
    auto endOutputCoilBit = startOutputCoilBit + outputCount;
    size_t portIndex = 0;

    std::vector<std::pair<QString, IOPortState>> changedStates;

    for (auto bitIndex = startInputCoilBit; bitIndex < endInputCoilBit; ++bitIndex)
    {
        auto portState = updatePortState(bitIndex, fetchedPortStates, portIndex);

        if (portState.isChanged)
        {
            changedStates.push_back(
                std::make_pair(m_ioStates[portIndex].id, portState.state));
        }

        portIndex++;
    }

    for (auto bitIndex = startOutputCoilBit; bitIndex < endOutputCoilBit; ++bitIndex)
    {
        auto portState = updatePortState(bitIndex, fetchedPortStates, portIndex);

        if (portState.isChanged)
        {
            changedStates.push_back(
                std::make_pair(m_ioStates[portIndex].id, portState.state));
        }

        portIndex++;
    }

    lock.unlock();
    for (const auto& change : changedStates)
    {
        m_inputStateChangedCallback(
            change.first,
            change.second);
    }
}

QnAdamModbusIOManager::PortStateChangeInfo QnAdamModbusIOManager::updatePortState(
    size_t bitIndex,
    const QByteArray& bytes,
    size_t portIndex)
{
    auto portId = m_ioStates[portIndex].id;
    auto currentState =
        nx_io_managment::fromBoolToIOPortState(getBitValue(bytes, bitIndex));

    bool isActive = getBitValue(bytes, bitIndex)
        != nx_io_managment::isActiveIOPortState(getPortDefaultStateUnsafe(portId));

    bool stateChanged = m_ioStates[portIndex].isActive != isActive;

    m_ioStates[portIndex].isActive = isActive;
    m_ioStates[portIndex].timestamp = qnSyncTime->currentMSecsSinceEpoch();

    return PortStateChangeInfo(currentState, stateChanged);
}

void QnAdamModbusIOManager::setDebounceForPort(const QString& portId, bool portState)
{
    QnMutexLocker lock(&m_mutex);

    DebouncedValue value;
    value.debouncedValue = portState;
    value.lifetimeCounter = kDebounceIterationCount;

    m_debouncedValues[portId] = value;
}

QnIOStateDataList QnAdamModbusIOManager::getDebouncedStates() const
{
    QnMutexLocker lock(&m_mutex);

    auto debouncedStates = m_ioStates;

    for (auto& state : debouncedStates)
    {
        if (m_debouncedValues.find(state.id) != m_debouncedValues.end())
        {
            state.isActive = m_debouncedValues[state.id].debouncedValue;
            if (!--m_debouncedValues[state.id].lifetimeCounter)
                m_debouncedValues.erase(state.id);
        }
    }

    return debouncedStates;
}

bool QnAdamModbusIOManager::getBitValue(const QByteArray& bytes, quint64 bitIndex) const
{
    const auto kBitsInByte = 8;

    int byteIndex = bitIndex / kBitsInByte;

    Q_ASSERT(byteIndex < bytes.size());

    auto byte = bytes[byteIndex];

    return !!(byte & ((quint8)1 << (bitIndex % kBitsInByte)));
}

void QnAdamModbusIOManager::scheduleMonitoringIteration()
{
    if (m_monitoringIsInProgress)
    {
        m_inputMonitorTimerId = nx::utils::TimerManager::instance()->addTimer(
            [this](quint64 timerId)
            {
                if (timerId == m_inputMonitorTimerId)
                    fetchAllPortStates();
            },
            std::chrono::milliseconds(kInputPollingIntervalMs));
    }
}

nx_io_managment::IOPortState QnAdamModbusIOManager::getPortDefaultStateUnsafe(const QString& portId) const
{
    if (m_defaultPortStates.count(portId))
        return m_defaultPortStates[portId];

    return kPortDefaultState;
}

void QnAdamModbusIOManager::routeMonitoringFlow(nx::modbus::ModbusMessage response)
{
    if (!m_monitoringIsInProgress)
        return;

    m_networkFaultsCounter = 0;

    processAllPortStatesResponse(response);
    scheduleMonitoringIteration();
}

void QnAdamModbusIOManager::handleMonitoringError()
{
    auto error = m_client.getLastErrorString();

    qDebug() << "Error occured << " << error;

    NX_LOG(error, cl_logDEBUG2);

    if (++m_networkFaultsCounter >= kMaxNetworkFaultsNumber)
    {
        m_networkFaultsCounter = 0;
        if (m_networkIssueCallback)
            m_networkIssueCallback(error, true);
    }

    if (m_monitoringIsInProgress)
        scheduleMonitoringIteration();
}

#endif //< ENABLE_ADVANTECH
