#include "modbus_async_client.h"
#include <utils/common/log.h>

using namespace nx_modbus;

namespace
{
    const int kSendTimeout = 4000;
    const int kRecvTimeout = 4000;
}

QnModbusAsyncClient::QnModbusAsyncClient()
{
    initSocket();
}

QnModbusAsyncClient::QnModbusAsyncClient(const SocketAddress& endpoint)
{

}

void QnModbusAsyncClient::setEndpoint(const SocketAddress& endpoint)
{
    m_endpoint = endpoint;
    initSocket();
}

void QnModbusAsyncClient::initSocket()
{
    if (m_socket)
    {
        m_socket->cancelAsyncIO();
        m_socket->shutdown();
    }

    m_socket = std::make_shared<TCPSocket>();

    if (!m_socket->setRecvTimeout(kRecvTimeout) ||
        !m_socket->setSendTimeout(kSendTimeout))
    {
        m_socket.reset();
    }

    m_sendBuffer.reserve(20000);
    m_recvBuffer.reserve(20000);
}

void QnModbusAsyncClient::readAsync()
{
    auto handler = std::bind(
        &QnModbusAsyncClient::onSomeBytesReadAsync,
        this,
        m_socket.get(),
        std::placeholders::_1,
        std::placeholders::_2);

    m_recvBuffer.truncate(0);

    qDebug() << "CAPCITY/SIZE" << m_recvBuffer.capacity() << m_recvBuffer.size();

    m_state = ModbusClientState::readingHeader;
    m_socket->readSomeAsync(&m_recvBuffer, handler);
}

void QnModbusAsyncClient::asyncSendDone(
    AbstractSocket *sock,
    SystemError::ErrorCode errorCode,
    size_t bytesWritten)
{

    if (errorCode != SystemError::noError)
    {
        m_lastErrorString = lit("ModbusAsyncClient: error while sending request. %1")
            .arg(errorCode);
        m_state = ModbusClientState::ready;
        emit error();
        return;
    }
    m_recvBuffer.truncate(0);
    readAsync();
}

void QnModbusAsyncClient::onSomeBytesReadAsync(
    AbstractSocket *sock,
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{

    if ( errorCode != SystemError::noError)
    {
        m_lastErrorString = lit("ModbusAsyncClient: error while reading response. %1")
            .arg(errorCode);
        m_state = ModbusClientState::ready;
        emit error();
    }

    processState();
}

void QnModbusAsyncClient::processState()
{
    if (m_state == ModbusClientState::ready
        || m_state == ModbusClientState::sendingMessage)
    {
        const auto logMessage = lit(
            "ModbusAsyncClient: Got unexpected data. It's not good");

        NX_LOG(logMessage, cl_logWARNING);
        qDebug() << logMessage;

        m_state = ModbusClientState::ready;
        m_lastErrorString = logMessage;
        emit error();

        return;
    }
    else if (m_state == ModbusClientState::readingHeader)
    {
        processHeader();
    }
    else if (m_state == ModbusClientState::readingData)
    {
        processData();
    }
}

void QnModbusAsyncClient::processHeader()
{
    Q_ASSERT(m_state == ModbusClientState::readingHeader);

    if (m_recvBuffer.size() < ModbusMBAPHeader::size)
        return;

    auto header = ModbusMBAPHeader::decode(m_recvBuffer);
    m_responseLength = header.length;

    if (m_recvBuffer.size() > ModbusMBAPHeader::size)
    {
        m_state = ModbusClientState::readingData;
        processData();
    }
}

void QnModbusAsyncClient::processData()
{

    Q_ASSERT(m_state == ModbusClientState::readingData);

    const quint16 fullMessageLength = ModbusMBAPHeader::size
        + m_responseLength
        - sizeof(decltype(ModbusMBAPHeader::unitId));

    if (m_recvBuffer.size() < fullMessageLength)
    {
        readAsync();
        return;
    }

    if (m_recvBuffer.size() > fullMessageLength)
    {
        const auto logMessage = 
            lit("ModbusAsyncClient: buffer size is more than expected response length.") 
            + lit("Buffer size: %1 bytes, response length: %2 bytes.")
            .arg(m_recvBuffer.size())
            .arg(fullMessageLength);

        NX_LOG(logMessage, cl_logWARNING);
        qDebug() << logMessage;

        m_lastErrorString = logMessage;
        emit error();
        return;
    }

    if (m_recvBuffer.size() == fullMessageLength)
    {
        auto response = ModbusResponse::decode(m_recvBuffer);
        m_state = ModbusClientState::ready;

        emit done(response);
    }
}

void QnModbusAsyncClient::doModbusRequestAsync(const ModbusRequest &request)
{
    m_sendBuffer.truncate(0);
    m_sendBuffer.append(ModbusRequest::encode(request));
    m_requestFunctionCode = request.functionCode;
    m_requestTransactionId = request.header.transactionId;

    auto handler = std::bind(
        &QnModbusAsyncClient::asyncSendDone,
        this,
        m_socket.get(),
        std::placeholders::_1,
        std::placeholders::_2);

    m_state = ModbusClientState::sendingMessage;

    if (!m_socket->isConnected())
        m_socket->connect(m_endpoint);

    m_socket->sendAsync(m_sendBuffer, handler);
}

void QnModbusAsyncClient::readCoilsAsync(quint16 startCoil, quint16 coilNumber)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kReadCoils;
    
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    
    stream << startCoil << coilNumber;

    request.data = data;
    request.header = buildHeader(request);

    doModbusRequestAsync(request);
}

ModbusMBAPHeader QnModbusAsyncClient::buildHeader(const ModbusRequest& request)
{
    ModbusMBAPHeader header;

    header.transactionId = ++m_requestTransactionId;
    header.protocolId = 0x00;
    header.unitId = 0x01;
    header.length = sizeof(header.unitId)
        + sizeof(request.functionCode)
        + request.data.size();

    return header;
}

QString QnModbusAsyncClient::getLastErrorString() const
{
    return m_lastErrorString;
}

