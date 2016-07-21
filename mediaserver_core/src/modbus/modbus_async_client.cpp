#include "modbus_async_client.h"
#include <utils/common/log.h>

using namespace nx_modbus;

namespace
{
    const int kSendTimeout = 4000;
    const int kRecvTimeout = 4000;
}

QnModbusAsyncClient::QnModbusAsyncClient():
    m_state(ModbusClientState::ready),
    m_requestSequenceNum(0),
    m_terminated(false)
{
    initSocket();
}

QnModbusAsyncClient::QnModbusAsyncClient(const SocketAddress& endpoint):
    m_state(ModbusClientState::ready),
    m_requestSequenceNum(false),
    m_terminated(false)
{
    setEndpoint(endpoint);
}

QnModbusAsyncClient::~QnModbusAsyncClient()
{
    if (m_socket)
    {
        m_socket->cancelAsyncIO();
        m_socket->shutdown();
    }
}

void QnModbusAsyncClient::setEndpoint(const SocketAddress& endpoint)
{
    m_endpoint = endpoint;
    initSocket();
}

void QnModbusAsyncClient::initSocket()
{
    m_sendBuffer.reserve(nx_modbus::kModbusMaxMessageLength);
    m_recvBuffer.reserve(nx_modbus::kModbusMaxMessageLength);

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
}

void QnModbusAsyncClient::readAsync(quint64 currentRequestSequenceNum)
{
    auto handler = 
        [currentRequestSequenceNum, this](SystemError::ErrorCode errorCode, size_t bytesRead)
        {
            QnMutexLocker lock(&m_mutex);

            if (m_terminated)
                return;

            if(currentRequestSequenceNum != m_requestSequenceNum)
                return;

            onSomeBytesReadAsync(m_socket.get(), currentRequestSequenceNum, errorCode, bytesRead);
        };
        
    m_socket->readSomeAsync(&m_recvBuffer, handler);
}

void QnModbusAsyncClient::asyncSendDone(
    AbstractSocket *sock,
    quint64 currentSequenceNum,
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

    m_state = ModbusClientState::readingHeader;
    
    readAsync(currentSequenceNum);
}

void QnModbusAsyncClient::onSomeBytesReadAsync(
    AbstractSocket *sock,
    quint64 currentRequestSequenceNum,
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

    processState(currentRequestSequenceNum);
}

void QnModbusAsyncClient::processState(quint64 currentRequestSequenceNum)
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
        processHeader(currentRequestSequenceNum);
    }
    else if (m_state == ModbusClientState::readingData)
    {
        processData(currentRequestSequenceNum);
    }
}

void QnModbusAsyncClient::processHeader(quint64 currentRequestSequenceNum)
{
    Q_ASSERT(m_state == ModbusClientState::readingHeader);

    if (m_recvBuffer.size() < ModbusMBAPHeader::size)
        return;

    auto header = ModbusMBAPHeader::decode(m_recvBuffer);
    m_responseLength = header.length;

    if (m_recvBuffer.size() > ModbusMBAPHeader::size)
    {
        m_state = ModbusClientState::readingData;
        processData(currentRequestSequenceNum);
    }
    else
    {
        readAsync(currentRequestSequenceNum);
    }
}

void QnModbusAsyncClient::processData(quint64 currentRequestSequenceNum)
{
    Q_ASSERT(m_state == ModbusClientState::readingData);

    const quint16 fullMessageLength = ModbusMBAPHeader::size
        + m_responseLength
        - sizeof(decltype(ModbusMBAPHeader::unitId));

    if (fullMessageLength > nx_modbus::kModbusMaxMessageLength)
    {
        m_lastErrorString = lit("Response message size is too big: %1 bytes")
            .arg(fullMessageLength);

        emit error();
    }

    if (m_recvBuffer.size() < fullMessageLength)
    {
        readAsync(currentRequestSequenceNum);
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

        if (!response.isException())
        {
            emit done(response);
        }
        else
        {
            m_lastErrorString = lit("ModbusAsyncClient, got exception: %1")
                .arg(response.getExceptionString());

            emit error();
        }
    }
}

void QnModbusAsyncClient::doModbusRequestAsync(const ModbusRequest &request)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    m_requestSequenceNum++;

    m_state = ModbusClientState::ready;
    m_socket->cancelAsyncIO();

    m_sendBuffer.truncate(0);
    m_recvBuffer.truncate(0);

    m_sendBuffer.append(ModbusRequest::encode(request));

    if (m_sendBuffer.size() > nx_modbus::kModbusMaxMessageLength)
    {
        m_lastErrorString = lit("Request size is too big: %1 bytes. Maximum request length is %2 bytes.")
            .arg(m_sendBuffer.size())
            .arg(nx_modbus::kModbusMaxMessageLength);

        emit error();
    }

    if (m_sendBuffer.isEmpty())
    {
        m_lastErrorString = lit("Serialized request is 0 bytes length.")
            .arg(m_sendBuffer.size())
            .arg(nx_modbus::kModbusMaxMessageLength);

        emit error();
    }


    m_requestFunctionCode = request.functionCode;
    m_requestTransactionId = request.header.transactionId;

    quint64 currentSequenceNum = m_requestSequenceNum;

    auto handler = 
        [currentSequenceNum, this]( 
            SystemError::ErrorCode errorCode, 
            size_t bytesWritten)
        {
            QnMutexLocker lock(&m_mutex);

            if (m_terminated)
                return;

            if (currentSequenceNum != m_requestSequenceNum)
                return;

            asyncSendDone(m_socket.get(), currentSequenceNum, errorCode, bytesWritten);
        };

    m_state = ModbusClientState::sendingMessage;

    if (!m_socket->isConnected())
    {   
        if (!m_socket->connect(m_endpoint))
        {
            m_lastErrorString = lit("ModbusAsyncClient, unable to connect to endpoint %1")
                .arg(m_endpoint.toString());
        }
    }

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

void QnModbusAsyncClient::terminate()
{
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
    }

    m_socket->terminateAsyncIO(true);
}

