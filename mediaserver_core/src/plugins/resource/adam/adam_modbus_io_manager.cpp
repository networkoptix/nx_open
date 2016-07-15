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
    const unsigned int kInputPollingIntervalMs = 500;
    const unsigned int kDefaultFirstInputCoilAddress = 0;
    const unsigned int kDefaultFirstOutputCoilAddress = 16;
}

QnAdamModbusIOManager::QnAdamModbusIOManager(QnResource* resource) :
    m_resource(resource),
    m_ioPortInfoFetched(false),
    m_firstInputCoilAddress(kDefaultFirstInputCoilAddress)
{
    initializeIO();
}

QnAdamModbusIOManager::~QnAdamModbusIOManager()
{
    stopIOMonitoring();
}

bool QnAdamModbusIOManager::startIOMonitoring()
{
    auto securityResource = dynamic_cast<QnSecurityCamResource*>(
        const_cast<QnResource*>(m_resource));

    if (!securityResource)
        return false;

    if (m_monitoringIsInProgress)
        return true;

    m_ioPortInfoFetched = initializeIO();

    if (!m_ioPortInfoFetched)
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


    auto host  = QUrl(m_resource->getUrl()).host();
    auto port = 502;
    SocketAddress endpoint(host, port);

    m_client.setEndpoint(endpoint);
    m_outputClient.setEndpoint(endpoint);

    fetchCurrentInputStates();

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

    //TODO: #dmishin check response for validity (likely inside client)

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

void QnAdamModbusIOManager::setInputPortStateChangeCallback(InputStateChangeCallback callback)
{
    m_inputStateChangedCallback = callback;
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

    qDebug() << "IO Manager, device vendor/model:" << securityResource->getVendor() << "/" << securityResource->getModel();

    auto resourceData = qnCommon->dataPool()->data(
        securityResource->getVendor(), 
        securityResource->getModel());

    auto ioPorts = resourceData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);

    for (const auto& ioPort: ioPorts)
    {
        if (ioPort.portType == Qn::PT_Input)
        {
            qDebug() << "Input port" << ioPort.inputName;
            m_inputs.push_back(ioPort);
        }
        else if (ioPort.portType == Qn::PT_Output)
        {
            qDebug() << "Output port" << ioPort.outputName;
            m_outputs.push_back(ioPort);
        }
        else
        {
            auto message = lit(
                "QnAdamModbusIOManager::fetchIOPorts, Unknown port type. Resource: %1, port ID: %2.")
                .arg(m_resource->getUniqueId())
                .arg(ioPort.id);

            NX_LOG(message, cl_logDEBUG2);
            qDebug() << message;
        }
    }

    m_ioPortInfoFetched = true;
    m_inputStates.resize(ioPorts.size());

    return true;
}

void QnAdamModbusIOManager::fetchCurrentInputStates()
{
    m_client.readCoilsAsync(m_firstInputCoilAddress, m_inputs.size());
}

void QnAdamModbusIOManager::processInputStatesResponse(const nx_modbus::ModbusResponse& response)
{
    if (response.data.isEmpty())
        return;

    // TODO: #dmishin check response for validity.
    auto countOfBytes = response.data[0];
    auto fetchedInputStates = response.data.mid(1);
    short kBitsInByte = 8;
    size_t inputIndex = 0;
    size_t inputCount = m_inputs.size();


    // Each bit in the response data represents single input port.
    for (const auto& byte: fetchedInputStates)
    {
        qDebug() << "DEBUG"  << sizeof(byte) << inputCount << inputIndex <<fetchedInputStates.toHex();
        for (short bitIndex = 0; (bitIndex < kBitsInByte) && (inputCount > inputIndex); ++bitIndex)
        {
            qDebug() << "State of input" << inputIndex << (byte & (1 << bitIndex));

            bool fetchedInputState = !!(byte & (1 << bitIndex));

            qDebug() << fetchedInputState << m_inputStates[inputIndex];

            if(fetchedInputState != m_inputStates[inputIndex])
            {
                IOPortState state = fetchedInputState ? 
                    IOPortState::active : IOPortState::nonActive;

                m_inputStates[inputIndex] = fetchedInputState;

                m_inputStateChangedCallback(m_inputs[inputIndex].id, state);
            }

            inputIndex++;
        }

        if(inputCount > inputIndex)
            break;
    }
}

void QnAdamModbusIOManager::scheduleMonitoringIteration()
{
    if (m_monitoringIsInProgress)
    {
        m_inputMonitorTimerId = TimerManager::instance()->addTimer(
            [this](quint64 timerId)
            {
                if (timerId == m_inputMonitorTimerId)
                    fetchCurrentInputStates();
            },
            kInputPollingIntervalMs);
    }
}

void QnAdamModbusIOManager::routeMonitoringFlow(nx_modbus::ModbusResponse response)
{
    processInputStatesResponse(response);
    scheduleMonitoringIteration();
}

void QnAdamModbusIOManager::handleMonitoringError()
{
    auto error = m_client.getLastErrorString();

    if (m_monitoringIsInProgress)
        scheduleMonitoringIteration();
}
