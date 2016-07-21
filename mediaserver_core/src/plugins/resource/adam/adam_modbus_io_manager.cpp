#ifdef ENABLE_ADVANTECH

#include <utils/common/log.h>
#include <core/resource/security_cam_resource.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <utils/common/synctime.h>
#include <utils/common/timermanager.h>

#include "adam_modbus_io_manager.h"

using namespace nx_io_managment;

namespace
{
    const unsigned int kInputPollingIntervalMs = 200;
    const unsigned int kDefaultFirstInputCoilAddress = 0;
    const unsigned int kDefaultFirstOutputCoilAddress = 16;

    const QString kAdamStartInputCoilParamName("adamStartInputCoil");
    const QString kAdamStartOutputCoilParamName("adamStartOutputCoil");
    const QString kAdamInputCountParamName("adamInputCount");
    const QString kAdamOutputCountParamName("adamOutputCount");
}

QnAdamModbusIOManager::QnAdamModbusIOManager(QnResource* resource) :
    m_resource(resource),
    m_ioPortInfoFetched(false)
{
    initializeIO();
}

QnAdamModbusIOManager::~QnAdamModbusIOManager()
{
    stopIOMonitoring();
    m_client.terminate();
}

bool QnAdamModbusIOManager::startIOMonitoring()
{
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

    QObject::connect(
        &m_client, &nx_modbus::QnModbusAsyncClient::done, 
        this, &QnAdamModbusIOManager::routeMonitoringFlow,
        Qt::DirectConnection);

    QObject::connect(
        &m_client, &nx_modbus::QnModbusAsyncClient::error, 
        this, &QnAdamModbusIOManager::handleMonitoringError,
        Qt::DirectConnection);

    m_monitoringIsInProgress = true;

    QUrl url(m_resource->getUrl());
    auto host  = url.host();
    auto port = url.port(nx_modbus::kDefaultModbusPort);

    SocketAddress endpoint(host, port);

    m_client.setEndpoint(endpoint);
    m_outputClient.setEndpoint(endpoint);

    fetchAllPortStates();

    return true;
}

void QnAdamModbusIOManager::stopIOMonitoring()
{
    m_monitoringIsInProgress = false;

    QObject::disconnect(
        &m_client, &nx_modbus::QnModbusAsyncClient::done, 
        this, &QnAdamModbusIOManager::routeMonitoringFlow);

    QObject::disconnect(
        &m_client, &nx_modbus::QnModbusAsyncClient::error, 
        this, &QnAdamModbusIOManager::handleMonitoringError);
}

bool QnAdamModbusIOManager::setOutputPortState(const QString& outputId, bool isActive)
{
    bool success = true;

    auto coil = getPortCoil(outputId, success);

    auto response = m_outputClient.writeSingleCoil(coil, isActive);

    return true;
}

bool QnAdamModbusIOManager::isMonitoringInProgress() const 
{
    return m_monitoringIsInProgress;
}

QnIOPortDataList QnAdamModbusIOManager::getInputPortList() const 
{
    return m_inputs;
}

QnIOPortDataList QnAdamModbusIOManager::getOutputPortList() const 
{
    return m_outputs;
}

QnIOStateDataList QnAdamModbusIOManager::getPortStates() const 
{
    QnMutexLocker lock(&m_mutex);
    return m_ioStates;
}

void QnAdamModbusIOManager::setInputPortStateChangeCallback(InputStateChangeCallback callback)
{
    m_inputStateChangedCallback = callback;
}

void QnAdamModbusIOManager::terminate()
{
    stopIOMonitoring();
    m_client.terminate();
}

quint32 QnAdamModbusIOManager::getPortCoil(const QString& ioPortId, bool& success) const
{
    auto split = ioPortId.split("_");

    if(split.size() != 2)
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

    auto resourceData = qnCommon->dataPool()->data(
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

void QnAdamModbusIOManager::fetchAllPortStates()
{
    bool status = true;
    auto startCoil = getPortCoil(m_inputs[0].id, status);
    auto lastCoil = getPortCoil(m_outputs[m_outputs.size() - 1].id, status);

    if (status)
        m_client.readCoilsAsync(startCoil, lastCoil - startCoil);
}

void QnAdamModbusIOManager::processAllPortStatesResponse(const nx_modbus::ModbusResponse& response)
{
    if (!m_monitoringIsInProgress)
        return;

    if (response.data.isEmpty())
        return;

    if (response.isException())
        return;

    bool status = true;
    short kBitsInByte = 8;

    auto inputCount = m_inputs.size();
    auto outputCount = m_outputs.size();
    
    auto fetchedPortStates = response.data.mid(1);
    quint64 bitsFetched = fetchedPortStates.size() * kBitsInByte;

    auto startInputCoilBit = getPortCoil(m_inputs[0].id, status);
    auto startOutputCoilBit = getPortCoil(m_outputs[0].id, status);

    if (!status)
        return;

    auto endInputCoilBit = startInputCoilBit + inputCount;
    auto endOutputCoilBit = startOutputCoilBit + outputCount;
    size_t portIndex = 0;

    for (auto bitIndex = startInputCoilBit; bitIndex < endInputCoilBit; ++bitIndex)
    {
        updatePortState(bitIndex, fetchedPortStates, portIndex);
        portIndex++;
    }

    for (auto bitIndex = startOutputCoilBit; bitIndex < endOutputCoilBit; ++bitIndex)
    {
        updatePortState(bitIndex, fetchedPortStates, portIndex);
        portIndex++;
    }
}

void QnAdamModbusIOManager::updatePortState(size_t bitIndex, const QByteArray& bytes, size_t portIndex)
{
    auto state = getBitValue(bytes, bitIndex);
    bool stateChanged = m_ioStates[portIndex].isActive == state;

    {
        QnMutexLocker lock(&m_mutex);
        m_ioStates[portIndex].isActive = state;
        m_ioStates[portIndex].timestamp = qnSyncTime->currentMSecsSinceEpoch();
    }

    nx_io_managment::IOPortState currentState = m_ioStates[portIndex].isActive ? 
        nx_io_managment::IOPortState::active : nx_io_managment::IOPortState::nonActive;

    if (stateChanged)
    {
        m_inputStateChangedCallback(
            m_ioStates[portIndex].id,
            currentState);
    }
}

bool QnAdamModbusIOManager::getBitValue(const QByteArray& bytes, quint64 bitIndex) const
{
    const auto kBitsInByte = 8;

    int byteIndex = bitIndex / kBitsInByte; 

    Q_ASSERT(byteIndex < bytes.size());

    auto byte = bytes[byteIndex];

    return !!(byte & (1 << bitIndex % kBitsInByte));
}

void QnAdamModbusIOManager::scheduleMonitoringIteration()
{
    if (m_monitoringIsInProgress)
    {
        m_inputMonitorTimerId = TimerManager::instance()->addTimer(
            [this](quint64 timerId)
            {
                if (timerId == m_inputMonitorTimerId)
                    fetchAllPortStates();
            },
            kInputPollingIntervalMs);
    }
}

void QnAdamModbusIOManager::routeMonitoringFlow(nx_modbus::ModbusResponse response)
{
    if (!m_monitoringIsInProgress)
        return;

    processAllPortStatesResponse(response);
    scheduleMonitoringIteration();
}

void QnAdamModbusIOManager::handleMonitoringError()
{
    auto error = m_client.getLastErrorString();

    if (m_monitoringIsInProgress)
        scheduleMonitoringIteration();
}

#endif //< ENABLE_ADVANTECH
