#include "modbus_async_client.h"
#include <nx/utils/log/log.h>

namespace
{
    const int kSendTimeout = 4000;
    const int kRecvTimeout = 4000;
}

namespace nx
{
namespace modbus
{

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
        post(std::bind(handler, SystemError::getLastOSErrorCode()));
        return;
    }

    m_modbusConnection.reset(
        new ModbusProtocolConnection(this, std::move(connectionSocket)));

    m_modbusConnection->setMessageHandler(
        [this](ModbusMessage message) { onMessage(std::move(message)); });

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

        onError(
            errorCode,
            lm("Error while connecting to endpoint %1.").str(m_endpoint));

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
        std::move(m_pendingMessage),
        [this](SystemError::ErrorCode errorCode)
        {
            if (errorCode != SystemError::noError)
                onError(errorCode, lit("Error while sending message."));
        });
}

void QnModbusAsyncClient::onMessage(ModbusMessage message)
{
    emit done(message);
}

void QnModbusAsyncClient::closeConnection(SystemError::ErrorCode closeReason, ConnectionType* connection)
{
    QN_UNUSED(connection);

    m_modbusConnection.reset();
    m_state = ModbusClientState::disconnected;
    
    onError(closeReason, lit("Connection closed."));
}

void QnModbusAsyncClient::doModbusRequestAsync(ModbusMessage message)
{
    post(
        [this, message = std::move(message)]()
        {
            m_pendingMessage = std::move(message);
            m_hasPendingMessage = true;

            if (m_state == ModbusClientState::disconnected)
                openConnection();
            else if (m_state == ModbusClientState::connected)
                sendPendingMessage();
        });
}

void QnModbusAsyncClient::onError(SystemError::ErrorCode errorCode, const QString& errorStr)
{
    {
        QnMutexLocker lock(&m_mutex);
        m_lastErrorString = errorStr + lm(" Error code: %1").str(errorCode);
    }

    emit error();
}

void QnModbusAsyncClient::readDiscreteInputsAsync(
    quint16 startRegister,
    quint16 registerCount,
    quint16* outTransactionId)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(registerCount);
    QN_UNUSED(outTransactionId);

    NX_ASSERT(false, "QnModbusAsyncClient::readDiscreteInputsAsync is not implemented");
}

void QnModbusAsyncClient::readCoilsAsync(
    quint16 startCoil,
    quint16 coilNumber,
    quint16* outTransactionId)
{
    ModbusMessage message;

    message.functionCode = FunctionCode::kReadCoils;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << startCoil << coilNumber;

    message.data = data;
    message.header = buildHeader(message);

    *outTransactionId = message.header.transactionId;
    doModbusRequestAsync(std::move(message));
}

void QnModbusAsyncClient::writeCoilsAsync(
    quint16 startCoilAddress,
    const QByteArray& data,
    quint16* outTransactionId)
{
    QN_UNUSED(startCoilAddress);
    QN_UNUSED(data);
    QN_UNUSED(outTransactionId);

    NX_ASSERT(false, "QnModbusAsyncClient::writeCoilsAsync is not implemented");
}

void QnModbusAsyncClient::readInputRegistersAsync(
    quint16 startRegister,
    quint16 registerCount,
    quint16* outTransactionId)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(registerCount);
    QN_UNUSED(outTransactionId);

    NX_ASSERT(false, "QnModbusAsyncClient::readInputRegisterAsync is not implemented");
}


void QnModbusAsyncClient::readHoldingRegistersAsync(
    quint32 startRegister,
    quint32 registerCount,
    quint16* outTransactionId)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(registerCount);
    QN_UNUSED(outTransactionId);

    NX_ASSERT(false, "QnModbusAsyncClient::readHoldingRegistersAsync is not implemented");
}

void QnModbusAsyncClient::writeHoldingRegistersAsync(
    quint32 startRegister,
    const QByteArray& data,
    quint16* outTransactionId)
{
    QN_UNUSED(startRegister);
    QN_UNUSED(data);
    QN_UNUSED(outTransactionId);

    NX_ASSERT(false, "QnModbusAsyncClient::writeHoldingRegistersAsync is not implemented");
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

void QnModbusAsyncClient::stopWhileInAioThread()
{
    m_modbusConnection.reset();
}

} //< Closing namespace modbus.

} //< Closing namespace nx.
