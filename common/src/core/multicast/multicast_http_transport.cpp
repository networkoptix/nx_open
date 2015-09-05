#include "multicast_http_transport.h"


namespace QnMulticast
{

static char CSV_DELIMITER = ';';
static char MESSAGE_BODY_DELIMITER = '$';
static const int MTU_SIZE = 1412;
static const QUuid PROTO_MAGIC("422DEA47-0B0C-439A-B1FA-19644CCBC0BD");
static const int PROTO_VERSION = 1;
static const QHostAddress MULTICAST_GROUP(QLatin1String("239.57.43.102"));
static const int MULTICAST_PORT = 7001;
static const int MAX_SEND_BUFFER_SIZE = 1024 * 64;
static const int PROCESSED_REQUESTS_CACHE_SZE = 512;
static const int SEND_RETRY_COUNT = 2;

// ----------- Packet ------------------

Packet::Packet():
    magic(PROTO_MAGIC), 
    version(PROTO_VERSION), 
    messageSize(0), 
    offset(0)
{

}

QByteArray Packet::serialize() const
{
    QByteArray result;
    result.append(magic.toString()).append(CSV_DELIMITER);          // magic ID CSV columnn
    result.append(QByteArray::number(version)).append(CSV_DELIMITER);          // magic ID CSV columnn
    result.append(requestId.toString()).append(CSV_DELIMITER);          // request ID CSV columnn
    result.append(clientId.toString()).append(CSV_DELIMITER);               // client ID CSV columnn
    result.append(serverId.toString()).append(CSV_DELIMITER);          // server ID CSV columnn
    result.append(QByteArray::number((int)messageType)).append(CSV_DELIMITER); // message type CSV columnn
    result.append(QByteArray::number(messageSize)).append(CSV_DELIMITER); // message size CSV columnn
    result.append(QByteArray::number(offset)).append(CSV_DELIMITER); // packet payload offset CSV columnn
    result.append(payloadData); // message body block CSV columnn
    
    return result;
}

Packet Packet::deserialize(const QByteArray& deserialize, bool* ok)
{
    Packet result;
    QList<QByteArray> fields = deserialize.split(CSV_DELIMITER);
    *ok = false;
    if (fields.size() != 9)
        return result; // error

    result.magic     = QUuid(fields[0]);
    result.version   = fields[1].toInt();

    if (result.magic != PROTO_MAGIC || result.version != PROTO_VERSION)
        return result; // error

    result.requestId   = QUuid(fields[2]);
    result.clientId    = QUuid(fields[3]);
    result.serverId    = QUuid(fields[4]);
    result.messageType = (MessageType) fields[5].toInt();
    result.messageSize = fields[6].toInt();
    result.offset      = fields[7].toInt();
    result.payloadData = fields[8];

    if (result.offset + result.payloadData.size() > result.messageSize)
        return result; // error

    *ok = true;
    return result;
}


// ----------------- Transport -----------------

Transport::Transport(const QUuid& localGuid):
    m_localGuid(localGuid),
    m_bytesWritten(0),
    m_processedRequests(PROCESSED_REQUESTS_CACHE_SZE)
{
    m_recvSocket.reset(new QUdpSocket());
    if (!m_recvSocket->bind(QHostAddress::AnyIPv4, MULTICAST_PORT, QAbstractSocket::ReuseAddressHint))
        qWarning() << "Failed to open Multicast Http receive socket";
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface: interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        m_recvSocket->joinMulticastGroup(MULTICAST_GROUP, iface);

        auto sendSocket = std::unique_ptr<QUdpSocket>(new QUdpSocket());
        if (sendSocket->bind())
            sendSocket->setMulticastInterface(iface);
        else
            qWarning() << "Failed to open Multicast Http send socket";

        connect(sendSocket.get(), &QUdpSocket::bytesWritten, this, &Transport::at_dataSent, Qt::QueuedConnection);
        m_sendSockets.push_back(std::move(sendSocket));
    }

    connect(m_recvSocket.get(), &QUdpSocket::readyRead, this, &Transport::at_socketReadyRead);

    m_timer.reset(new QTimer());
    connect(m_timer.get(), &QTimer::timeout, this, &Transport::at_timer);
    m_timer->start(1000);
}

void Transport::at_dataSent(qint64 bytes)
{
    m_bytesWritten = qMax(0ll, m_bytesWritten - bytes);
    if (m_bytesWritten == 0) 
    {
        // cleanup request if all data are sent and response isn't expected (for server side)
        QMutexLocker lock(&m_mutex);
        for (auto itr = m_requests.begin(); itr != m_requests.end();)
        {
            TransportConnection& request = *itr;
            if (request.dataToSend.isEmpty() && !request.responseCallback)
                itr = m_requests.erase(itr);
            else
                ++itr;
        }
    }
    if (m_bytesWritten == 0) 
        sendNextData();
}

void Transport::at_timer()
{
    QMutexLocker lock(&m_mutex);
    for (auto itr = m_requests.begin(); itr != m_requests.end();)
    {
        TransportConnection& request = *itr;
        if (request.hasExpired()) {
            if (request.responseCallback)
                request.responseCallback(request.requestId, ErrCode::timeout, Response());
            itr = m_requests.erase(itr);
        }
        else
            ++itr;
    }
}

QByteArray Transport::encodeMessage(const Request& request) const
{
    QString result;

    QString encodedPath = request.url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters | QUrl::RemoveScheme | QUrl::RemoveAuthority);

    result.append(lit("%1 %2 HTTP/1.1\r\n").arg(request.method).arg(encodedPath));
    for (const auto& header: request.extraHttpHeaders)
        result.append(lit("%1: %2\r\n").arg(header.first).arg(header.second));
    if (!request.contentType.isEmpty())
        result.append(lit("%1: %2\r\n").arg(lit("Content-Type")).arg(QLatin1String(request.contentType)));
    result.append(lit("\r\n"));
    return result.toUtf8().append(request.messageBody);
}

QnMulticast::Response Transport::decodeResponse(const TransportConnection& transportData, bool* ok) const
{
    *ok = false;
    Response response;

    QByteArray message = QByteArray::fromBase64(transportData.receivedData);
    // extract http result
    int requestLineEnd = message.indexOf("\r\n");
    if (requestLineEnd == -1)
        return response; // error
    QByteArray requestLine = message.mid(0, requestLineEnd);
    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 3)
        return response; // error
    response.httpResult = parts[1].toInt();
    int msgBodyPos = message.indexOf("\r\n\r\n");
    if (msgBodyPos == -1)
        msgBodyPos = message.size();
    // parse headers
    int headersStart = requestLineEnd + 2;
    QByteArray headers = QByteArray::fromRawData(message.data() + headersStart, msgBodyPos - headersStart);
    for (const auto& header: headers.split('\n'))
    {
        int delimiterPos = header.indexOf(':');
        if (delimiterPos != -1) {
            QByteArray headerName = header.left(delimiterPos).trimmed();
            if (headerName.toLower() == "content-type") {
                response.contentType = header.mid(delimiterPos+1).trimmed();
            }
        }
    }
    response.messageBody = message.mid(msgBodyPos + 4);
    
    return response;
}

QnMulticast::Request Transport::decodeRequest(const TransportConnection& transportData, bool* ok) const
{
    *ok = false;
    QnMulticast::Request request;
    request.serverId = m_localGuid;
    QByteArray message = QByteArray::fromBase64(transportData.receivedData);
    // extract http result
    int requestLineEnd = message.indexOf("\r\n");
    if (requestLineEnd == -1)
        return request; // error
    QByteArray requestLine = message.mid(0, requestLineEnd);
    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 3)
        return request; // error
    request.method = QString::fromLatin1(parts[0]);
    request.url = QLatin1String(parts[1]);
    int msgBodyPos = message.indexOf("\r\n\r\n");
    if (msgBodyPos == -1)
        msgBodyPos = message.size();
    // parse headers
    int headersStart = requestLineEnd + 2;
    QByteArray headers = QByteArray::fromRawData(message.data() + headersStart, msgBodyPos - headersStart);
    for (const auto& header: headers.split('\n'))
    {
        int delimiterPos = header.indexOf(':');
        if (delimiterPos != -1) {
            QPair<QString,QString> h;
            h.first = QLatin1String(header.left(delimiterPos).trimmed());
            h.second = QLatin1String(header.right(delimiterPos+1).trimmed());
            if (h.first.toLower() == lit("content-type"))
                request.contentType = h.second.toUtf8();
            else
                request.extraHttpHeaders << h;
        }
    }
    request.messageBody = message.mid(msgBodyPos + 4);
    *ok = true;
    return request;
}

QUuid Transport::addRequest(const Request& request, ResponseCallback callback, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);
    TransportConnection transportRequest = encodeRequest(request);
    transportRequest.responseCallback = callback;
    transportRequest.timeoutMs = timeoutMs;
    m_requests[transportRequest.requestId] = std::move(transportRequest);
    QMetaObject::invokeMethod(this, "sendNextData", Qt::QueuedConnection);
    return transportRequest.requestId;
}

void Transport::putPacketToTransport(TransportConnection& transportConnection, const Packet& packet)
{
    QByteArray encodedData = packet.serialize();
    for (int i = 0; i < SEND_RETRY_COUNT; ++i) 
    {
        for (auto& socket: m_sendSockets)
            transportConnection.dataToSend << TransportPacket(socket.get(), encodedData);
    }
}

Transport::TransportConnection Transport::encodeRequest(const Request& request)
{
    TransportConnection transportRequest;
    QByteArray message = encodeMessage(request).toBase64();
    transportRequest.requestId = QUuid::createUuid();
    for (int offset = 0; offset < message.size();)
    {
        Packet packet;
        packet.requestId = transportRequest.requestId;
        packet.clientId = m_localGuid;
        packet.serverId = request.serverId;
        packet.messageType = MessageType::request;
        packet.messageSize = message.size();
        packet.offset = offset;
        int payloadSize = qMin(Packet::MAX_PAYLOAD_SIZE, message.size() - offset);
        packet.payloadData = message.mid(offset, payloadSize);
        putPacketToTransport(transportRequest, packet);
        offset += payloadSize;
    }

    return transportRequest;
}

void Transport::addResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse)
{
    QMutexLocker lock(&m_mutex);
    m_requests[requestId] = encodeResponse(requestId, clientId, httpResponse);
    QMetaObject::invokeMethod(this, "sendNextData", Qt::QueuedConnection);
}

Transport::TransportConnection Transport::encodeResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse)
{
    TransportConnection transportConnection;
    transportConnection.requestId = requestId;
    QByteArray message = httpResponse.toBase64();

    for (int offset = 0; offset < message.size();)
    {
        Packet packet;
        packet.requestId = requestId;
        packet.serverId = m_localGuid;
        packet.clientId = clientId;
        packet.messageType = MessageType::response;
        packet.messageSize = message.size();
        packet.offset = offset;
        int payloadSize = qMin(Packet::MAX_PAYLOAD_SIZE, message.size() - offset);
        packet.payloadData = message.mid(offset, payloadSize);
        putPacketToTransport(transportConnection, packet);
        offset += payloadSize;
    }

    return transportConnection;
}

void Transport::at_socketReadyRead()
{
    QMutexLocker lock(&m_mutex);
    while (m_recvSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(m_recvSocket->pendingDatagramSize());
        int readed = m_recvSocket->readDatagram(datagram.data(), datagram.size());
        if (readed <= 0)
            return;
        bool ok;
        Packet packet = Packet::deserialize(datagram, &ok);
        if (ok)
        {
            if (packet.messageType == MessageType::response && !m_requests.contains(packet.requestId))
                continue; // ignore response without request (probably request already removed because of timeout or other reason)

            if (packet.messageType == MessageType::request && packet.serverId != m_localGuid)
                continue; // foreign packet
            if (packet.messageType == MessageType::response && packet.clientId != m_localGuid)
                continue; // foreign packet
            if (m_processedRequests.contains(packet.requestId))
                continue; // request already processed

            TransportConnection& transportData = m_requests[packet.requestId];
            transportData.requestId = packet.requestId;
            if (transportData.receivedData.isEmpty()) {
                transportData.receivedData.resize(packet.messageSize);
                transportData.receivedData.fill('\x0');
            }
            memcpy(transportData.receivedData.data() + packet.offset, packet.payloadData.data(), packet.payloadData.size());
            if (!transportData.receivedData.contains('\x0'))
            {
                m_processedRequests.insert(packet.requestId, 0);
                // all data has been received
                bool ok;
                if (packet.messageType == MessageType::response) 
                {
                    Response response = decodeResponse(transportData, &ok);
                    response.serverId = packet.serverId;
                    transportData.responseCallback(transportData.requestId, ok ? ErrCode::ok : ErrCode::networkIssue, response);
                    m_requests.remove(packet.requestId);
                }
                else if (packet.messageType == MessageType::request) 
                {
                    Request request = decodeRequest(transportData, &ok);
                    if (ok)
                        m_requestCallback(transportData.requestId, packet.clientId, request);
                }
            }
        }
    }
}

void Transport::sendNextData()
{
    QMutexLocker lock(&m_mutex);
    int prevBytesWritten = -1;
    for (auto itr = m_requests.begin(); itr != m_requests.end(); ++itr)
    {
        TransportConnection& request = *itr;
        while (m_bytesWritten < MAX_SEND_BUFFER_SIZE && prevBytesWritten != m_bytesWritten && !request.dataToSend.isEmpty())
        {
            prevBytesWritten = m_bytesWritten;
            TransportPacket data = request.dataToSend.dequeue();
            int bytesWriten = data.socket->writeDatagram(data.data, MULTICAST_GROUP, MULTICAST_PORT);
            if (bytesWriten == -1) {
                if (request.responseCallback)
                    request.responseCallback(request.requestId, ErrCode::networkIssue, Response());
                request.dataToSend.clear();
            }
            else {
                m_bytesWritten += bytesWriten;
            }
        }
    }

}

}
