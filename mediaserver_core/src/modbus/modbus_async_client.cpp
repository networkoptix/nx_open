#include "modbus_async_client.h"
#include <nx/utils/log/log.h>

using namespace nx_modbus;

namespace
{
    const int kSendTimeout = 4000;
    const int kRecvTimeout = 4000;
}

QnModbusAsyncClient::QnModbusAsyncClient():
    m_state(ModbusClientState::disconnected),
    m_modbusConnection(nullptr),
    m_requestSequenceNumber(0),
    m_hasPendingMessage(false)
{
}

QnModbusAsyncClient::QnModbusAsyncClient(const SocketAddress& endpoint):  
    m_state(ModbusClientState::disconnected),
    m_modbusConnection(nullptr),
    m_requestSequenceNumber(0),
    m_hasPendingMessage(false),
    m_endpoint(endpoint)
{
}

QnModbusAsyncClient::~QnModbusAsyncClient()
{
    stopWhileInAioThread();
}

void QnModbusAsyncClient::setEndpoint(const SocketAddress& endpoint)
{
    post(
        [this, endpoint]()
        {
            if (endpoint == m_endpoint)
                return;

            m_modbusConnection.reset();
            m_state = ModbusClientState::disconnected;
            m_endpoint = endpoint;
            m_hasPendingMessage = false;
            m_requestSequenceNumber = 0;
        });
}

void QnModbusAsyncClient::openConnection()
{
    if (m_endpoint.isNull())
        return;

    if (m_state != ModbusClientState::disconnected)
        return;

    auto handler = [this](SystemError::ErrorCode errorCode){ onConnectionDone(errorCode); };
    
    bool kSslNeeded = false;
    auto connectionSocket = SocketFactory::createStreamSocket(
        kSslNeeded, SocketFactory::NatTraversalType::nttDisabled);

    connectionSocket->bindToAioThread(getAioThread());

    if (!connectionSocket->setNonBlockingMode(true) ||
        !connectionSocket->setSendTimeout(kSendTimeout) ||
        !connectionSocket->setRecvTimeout(kRecvTimeout))
    {
        connectionSocket->post(
            std::bind(handler, SystemError::getLastOSErrorCode()));

        return;
    }

    m_modbusConnection.reset(
        new ModbusProtocolConnection(this, std::move(connectionSocket)));

    m_modbusConnection->setMessageHandler(
        [this](ModbusMessage message) { onMessage(message); });

    m_state = ModbusClientState::connecting;

    m_modbusConnection->socket()->connectAsync(m_endpoint, handler);
}


void QnModbusAsyncClient::onConnectionDone(SystemError::ErrorCode errorCode)
{
    if (m_state != ModbusClientState::connecting)
        return;

    if (errorCode != SystemError::noError)
    {
        m_state = ModbusClientState::disconnected;

        onError(errorCode, lit("Error while connecting to endpoint %1.")
            .arg(m_endpoint.toString()));

        return;
    }

    m_state = ModbusClientState::connected;

    m_modbusConnection->startReadingConnection();

    sendPendingMessage();
}

void QnModbusAsyncClient::sendPendingMessage()
{
    NX_ASSERT(m_state == ModbusClientState::connected);

    if (!m_hasPendingMessage)
        return;

    m_hasPendingMessage = false;
    m_modbusConnection->sendMessage(
        m_pendingMessage,
        [this](SystemError::ErrorCode errorCode)
        {
            if (errorCode != SystemError::noError)
                onError(errorCode, lit("Error while sending message."));
        });
}

void QnModbusAsyncClient::onMessage(ModbusMessage message)
{
    // Looks strange, but needed because doModbusRequestAsync should return transaction id
    // before 'done' signal will be emitted.
    { QnMutexLocker lock(&m_mutex); }
    
    emit done(message);
}

void QnModbusAsyncClient::closeConnection(SystemError::ErrorCode closeReason, ConnectionType* connection)
{
    QN_UNUSED(connection);

    m_modbusConnection.reset();
    m_state = ModbusClientState::disconnected;
    
    onError(closeReason, lit("Connection closed."));
}

void QnModbusAsyncClient::doModbusRequestAsync(const ModbusMessage &message)
{
    post(
        [this, message]()
        {
            m_pendingMessage = message;
            m_hasPendingMessage = true;

            if (m_state == ModbusClientState::disconnected)
                openConnection();
            else if (m_state == ModbusClientState::connected)
                sendPendingMessage();
        });
}

void QnModbusAsyncClient::onError(SystemError::ErrorCode errorCode, QString& errorStr)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_lastErrorString = errorStr.append(
            lit(" Error code: %1").arg(errorCode));
    }

    emit error();
}

quint16 QnModbusAsyncClient::readDiscreteInputsAsync(quint16 startRegister, quint16 registerCount)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(registerCount);

    NX_ASSERT(false, "QnModbusAsyncClient::readDiscreteInputsAsync is not implemented");

    return 0;
}

quint16 QnModbusAsyncClient::readCoilsAsync(quint16 startCoil, quint16 coilNumber)
{
    ModbusMessage message;

    message.functionCode = FunctionCode::kReadCoils;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << startCoil << coilNumber;

    message.data = data;
    message.header = buildHeader(message);

    {
        QnMutexLocker lock(&m_mutex);
        doModbusRequestAsync(message);
        return message.header.transactionId;
    }
}

quint16 QnModbusAsyncClient::writeCoilsAsync(quint16 startCoilAddress, const QByteArray& data)
{
    QN_UNUSED(startCoilAddress);
    QN_UNUSED(data);

    NX_ASSERT(false, "QnModbusAsyncClient::writeCoilsAsync is not implemented");

    return 0;
}

quint16 QnModbusAsyncClient::readInputRegistersAsync(quint16 startRegister, quint16 registerCount)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(registerCount);

    NX_ASSERT(false, "QnModbusAsyncClient::readInputRegisterAsync is not implemented");

    return 0;
}


quint16 QnModbusAsyncClient::readHoldingRegistersAsync(quint32 startRegister, quint32 registerCount)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(registerCount);

    NX_ASSERT(false, "QnModbusAsyncClient::readHoldingRegistersAsync is not implemented");

    return 0;
}

quint16 QnModbusAsyncClient::writeHoldingRegistersAsync(quint32 startRegister, const QByteArray& data)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(data);

    NX_ASSERT(false, "QnModbusAsyncClient::writeHoldingRegistersAsync is not implemented");

    return 0;
}

ModbusMBAPHeader QnModbusAsyncClient::buildHeader(const ModbusMessage& request)
{
    ModbusMBAPHeader header;

    header.transactionId = ++m_requestSequenceNumber;
    header.protocolId = 0x00;
    header.unitId = 0x01;
    header.length = sizeof(header.unitId)
        + sizeof(request.functionCode)
        + request.data.size();

    return header;
}

QString QnModbusAsyncClient::getLastErrorString() const
{
    QnMutexLocker lock(&m_mutex);
    return m_lastErrorString;
}

void nx_modbus::QnModbusAsyncClient::stopWhileInAioThread()
{
    m_modbusConnection.reset();
}
