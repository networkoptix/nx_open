#include "multicast_http_transport.h"


namespace QnMulticast
{

static const QUuid PROTO_MAGIC("422DEA47-0B0C-439A-B1FA-19644CCBC0BD");
static const int PROTO_VERSION = 1;

static const int MTU_SIZE = 1412;
static const QHostAddress MULTICAST_GROUP(QLatin1String("239.57.43.102"));
static const int MULTICAST_PORT = 7001;

static const int MAX_SEND_BUFFER_SIZE = 1024 * 8;     // how many bytes we send before socket delay
static const int SOCKET_RECV_BUFFER_SIZE = 1024 * 64; // socket read buffer size
static const int SEND_RETRY_COUNT = 2;                // how many times send all UDP packets
static const int SEND_BITRATE = 1024*1024 * 2;        // send data at speed 2Mbps

static const int PROCESSED_REQUESTS_CACHE_SZE = 512;  // keep last sessions to ignore UDP duplicates
static const int CSV_COLUMNS = 9;
static char CSV_DELIMITER = ';';
static char EMPTY_DATA_FILLER = '\x0';

// --------- UDP / Multicast routines -----------------

bool setRecvBufferSize(int fd, unsigned int buff_size )
{
    return ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*) &buff_size, sizeof(buff_size)) == 0;
}

static bool joinMulticastGroup(int fd, const QString &multicastGroup, const QString& multicastIF)  
{
    struct ip_mreq multicastRequest;

    multicastRequest.imr_multiaddr.s_addr = inet_addr(multicastGroup.toLatin1());
    multicastRequest.imr_interface.s_addr = inet_addr(multicastIF.toLatin1());
    if( setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        (const char *) &multicastRequest,
        sizeof(multicastRequest)) < 0) {
            qWarning() << "failed to join multicast group" << multicastGroup << "from IF" << multicastIF;
            return false;
    }
    return true;
}

QString getIfaceIPv4Addr(const QNetworkInterface& iface)
{
    for (const auto& addr: iface.addressEntries())
    {
        if (QAbstractSocket::IPv4Protocol == addr.ip().protocol() && // if it has IPV4
            addr.ip()!=QHostAddress::LocalHost &&// if this is not 0.0.0.0 or 127.0.0.1
            addr.netmask().toIPv4Address()!=0) // and mask !=0
            return addr.ip().toString();
    }
    return QString();
}

bool setMulticastIF(int fd, const QString& multicastIF)
{
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr(multicastIF.toLatin1().data());
    if( setsockopt( fd, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&localInterface, sizeof( localInterface ) ) < 0 )
    {
        qWarning() << "IP_MULTICAST_IF set failed for iface " << multicastIF;
        return false;
    }
    return true;
}

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
    result.append(magic.toString()).append(CSV_DELIMITER);             // magic
    result.append(QByteArray::number(version)).append(CSV_DELIMITER);  // protocol version
    result.append(requestId.toString()).append(CSV_DELIMITER);         // request ID
    result.append(clientId.toString()).append(CSV_DELIMITER);          // client ID
    result.append(serverId.toString()).append(CSV_DELIMITER);          // server ID
    result.append(QByteArray::number((int)messageType)).append(CSV_DELIMITER); // message type
    result.append(QByteArray::number(messageSize)).append(CSV_DELIMITER); // message size
    result.append(QByteArray::number(offset)).append(CSV_DELIMITER); // packet payload offset
    result.append(payloadData); // message body
    
    return result;
}

Packet Packet::deserialize(const QByteArray& deserialize, bool* ok)
{
    Packet result;
    QList<QByteArray> fields = deserialize.split(CSV_DELIMITER);
    *ok = false;
    if (fields.size() != CSV_COLUMNS)
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
    m_processedRequests(PROCESSED_REQUESTS_CACHE_SZE),
    m_mutex(QMutex::Recursive),
    m_nextSendQueued(false),
    m_initialized(false)
{
    m_recvSocket.reset(new QUdpSocket());
    if (!m_recvSocket->bind(QHostAddress::AnyIPv4, MULTICAST_PORT, QAbstractSocket::ReuseAddressHint))
        qWarning() << "Failed to open Multicast Http receive socket";
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    bool hasRecvIface = false;
    bool hasSendIface = false;
    for (const QNetworkInterface &iface: interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        // QT joinMulticastGroup has a bug: it takes first address from the interface. It fail if the first addr is a IPv6 addr
        //m_recvSocket->joinMulticastGroup(MULTICAST_GROUP, iface);

        QString ipv4Addr = getIfaceIPv4Addr(iface);
        if (ipv4Addr.isEmpty())
            continue; // no IPv4 address found
        
        if (joinMulticastGroup(m_recvSocket->socketDescriptor(), MULTICAST_GROUP.toString(), ipv4Addr))
            hasRecvIface = true;

        auto sendSocket = std::unique_ptr<QUdpSocket>(new QUdpSocket());
        if (sendSocket->bind(QHostAddress(ipv4Addr))) 
        {
            setMulticastIF(sendSocket->socketDescriptor(), ipv4Addr); //sendSocket->setMulticastInterface(iface);
            m_sendSockets.push_back(std::move(sendSocket));
            hasSendIface = true;
            if (m_localAddress.isEmpty())
                m_localAddress = ipv4Addr;
        }
        else
            qWarning() << "Failed to open Multicast Http send socket";
    }
    setRecvBufferSize(m_recvSocket->socketDescriptor(), SOCKET_RECV_BUFFER_SIZE);
    connect(m_recvSocket.get(), &QUdpSocket::readyRead, this, &Transport::at_socketReadyRead);
    m_initialized = hasSendIface && hasRecvIface;
    m_timer.reset(new QTimer());
    connect(m_timer.get(), &QTimer::timeout, this, &Transport::at_timer);
    m_timer->start(1000);
}

QString Transport::localAddress() const
{
    return m_localAddress;
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
            m_processedRequests.insert(request.requestId, 0);
            itr = m_requests.erase(itr);
        }
        else
            ++itr;
    }
}

void Transport::cancelRequest(const QUuid& requestId)
{
    QMutexLocker lock(&m_mutex);
    eraseRequest(requestId);
    m_processedRequests.insert(requestId, 0);
}

QByteArray Transport::serializeMessage(const Request& request) const
{
    QString result;

    QString encodedPath = request.url.toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters | QUrl::RemoveScheme | QUrl::RemoveAuthority);

    result.append(QStringLiteral("%1 %2 HTTP/1.1\r\n").arg(request.method).arg(encodedPath));
    for (const auto& header: request.headers)
        result.append(QStringLiteral("%1: %2\r\n").arg(header.first).arg(header.second));
    if (!request.contentType.isEmpty())
        result.append(QStringLiteral("%1: %2\r\n").arg(QLatin1String("Content-Type")).arg(QLatin1String(request.contentType)));
    result.append(QLatin1String("\r\n"));
    return result.toUtf8().append(request.messageBody);
}

bool Transport::parseHttpMessage(Message& result, const QByteArray& message) const
{
    static const QByteArray ENDL("\r\n");
    static const QByteArray DOUBLE_ENDL("\r\n\r\n");

    // extract http result
    int requestLineEnd = message.indexOf(ENDL);
    if (requestLineEnd == -1)
        return false; // error
    int msgBodyPos = message.indexOf(DOUBLE_ENDL);
    if (msgBodyPos == -1)
        msgBodyPos = message.size();
    // parse headers
    int headersStart = requestLineEnd + ENDL.size();
    QByteArray headers = QByteArray::fromRawData(message.data() + headersStart, msgBodyPos - headersStart);
    for (const auto& header: headers.split('\n'))
    {
        int delimiterPos = header.indexOf(':');
        if (delimiterPos != -1) {
            Header h;
            h.first = QLatin1String(header.left(delimiterPos).trimmed());
            h.second = QLatin1String(header.mid(delimiterPos+1).trimmed());
            if (h.first.toLower() == QLatin1String("content-type"))
                result.contentType = header.mid(delimiterPos+1).trimmed();
            else
                result.headers << h;
        }
    }
    result.messageBody = message.mid(msgBodyPos + DOUBLE_ENDL.size());
    return true;
}

QnMulticast::Response Transport::parseResponse(const TransportConnection& transportData, bool* ok) const
{
    *ok = false;
    Response response;

    QByteArray message = QByteArray::fromBase64(transportData.receivedData);
    if (!parseHttpMessage(response, message))
        return response;

    // extract http result
    QByteArray requestLine = message.left(message.indexOf("\r\n"));
    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 3)
        return response; // error
    response.httpResult = parts[1].toInt();
    *ok = true;
    return response;
}

QnMulticast::Request Transport::parseRequest(const TransportConnection& transportData, bool* ok) const
{
    *ok = false;
    QnMulticast::Request request;
    request.serverId = m_localGuid;
    QByteArray message = QByteArray::fromBase64(transportData.receivedData);
    if (!parseHttpMessage(request, message))
        return request;

    // extract http result
    QByteArray requestLine = message.left(message.indexOf("\r\n"));
    QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 3)
        return request; // error
    request.method = QLatin1String(parts[0]);
    request.url = QLatin1String(parts[1]);
    *ok = true;
    return request;
}

QUuid Transport::addRequest(const Request& request, ResponseCallback callback, int timeoutMs)
{
    QMutexLocker lock(&m_mutex);
    TransportConnection transportRequest = serializeRequest(request);
    transportRequest.responseCallback = callback;
    transportRequest.timeoutMs = timeoutMs;
    m_requests.push_back(std::move(transportRequest));
    queueNextSendData(0);
    return transportRequest.requestId;
}

void Transport::putPacketToTransport(TransportConnection& transportConnection, const Packet& packet)
{
    QByteArray encodedData = packet.serialize();
    Q_ASSERT(encodedData.size() <= Packet::MAX_DATAGRAM_SIZE);
    for (int i = 0; i < SEND_RETRY_COUNT; ++i) 
    {
        for (auto& socket: m_sendSockets)
            transportConnection.dataToSend << TransportPacket(socket.get(), encodedData);
    }
}

Transport::TransportConnection Transport::serializeRequest(const Request& request)
{
    TransportConnection transportRequest;
    QByteArray message = serializeMessage(request).toBase64();
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
    m_requests.push_back(serializeResponse(requestId, clientId, httpResponse));
    queueNextSendData(0);
}

Transport::TransportConnection Transport::serializeResponse(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpResponse)
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

void Transport::eraseRequest(const QUuid& id)
{
    m_requests.erase(std::remove_if(m_requests.begin(), m_requests.end(), [id](const TransportConnection& conn) {
        return conn.requestId == id;
    }));
}

void Transport::at_socketReadyRead()
{
    QMutexLocker lock(&m_mutex);
    while (m_recvSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(m_recvSocket->pendingDatagramSize());
        int gotBytes = m_recvSocket->readDatagram(datagram.data(), datagram.size());
        if (gotBytes <= 0)
            continue;
        bool ok;
        Packet packet = Packet::deserialize(datagram, &ok);
        if (!ok)
            continue;

        auto itr = std::find_if(m_requests.begin(), m_requests.end(), [packet](const TransportConnection& conn) {
            return conn.requestId == packet.requestId;
        });

        if (packet.messageType == MessageType::response && itr == m_requests.end())
            continue; // ignore response without request (probably request already removed because of timeout or other reason)

        if (packet.messageType == MessageType::request && packet.serverId != m_localGuid)
            continue; // foreign packet
        if (packet.messageType == MessageType::response && packet.clientId != m_localGuid)
            continue; // foreign packet
        if (m_processedRequests.contains(packet.requestId))
            continue; // request already processed

        if (itr == m_requests.end()) {
            m_requests.push_back(TransportConnection());
            itr = m_requests.end()-1;
        }
        TransportConnection& transportData = *itr;
        transportData.requestId = packet.requestId;
        if (transportData.receivedData.isEmpty()) {
            transportData.receivedData.resize(packet.messageSize);
            transportData.receivedData.fill(EMPTY_DATA_FILLER);
        }
        memcpy(transportData.receivedData.data() + packet.offset, packet.payloadData.data(), packet.payloadData.size());
        if (!transportData.receivedData.contains(EMPTY_DATA_FILLER))
        {
            m_processedRequests.insert(packet.requestId, 0);
            // all data has been received
            bool ok;
            if (packet.messageType == MessageType::response) 
            {
                Response response = parseResponse(transportData, &ok);
                response.serverId = packet.serverId;
                if (transportData.responseCallback)
                    transportData.responseCallback(transportData.requestId, ok ? ErrCode::ok : ErrCode::networkIssue, response);
            }
            else if (packet.messageType == MessageType::request) 
            {
                Request request = parseRequest(transportData, &ok);
                if (ok && m_requestCallback)
                    m_requestCallback(transportData.requestId, packet.clientId, request);
            }
            eraseRequest(packet.requestId);
        }
    }
}

void Transport::sendNextData()
{
    QMutexLocker lock(&m_mutex);
    m_nextSendQueued = false;
    int sentBytes = 0;
    for (auto itr = m_requests.begin(); itr != m_requests.end();)
    {
        TransportConnection& request = *itr;

        if (!m_initialized) {
            if (request.responseCallback)
                request.responseCallback(request.requestId, ErrCode::networkIssue, Response());
            itr = m_requests.erase(itr);
            continue;
        }

        while (sentBytes < MAX_SEND_BUFFER_SIZE && !request.dataToSend.isEmpty())
        {
            TransportPacket data = request.dataToSend.dequeue();
            int bytesWriten = data.socket->writeDatagram(data.data, MULTICAST_GROUP, MULTICAST_PORT);
            /*
            if (bytesWriten == -1) {
                if (request.responseCallback)
                    request.responseCallback(request.requestId, ErrCode::networkIssue, Response());
                request.dataToSend.clear();
            }
            else {
                sentBytes += bytesWriten;
            }
            */
            if (bytesWriten > 0)
                sentBytes += Packet::MAX_DATAGRAM_SIZE; // take into account max datagram size for bitrate calculation
        }
        
        if (request.dataToSend.isEmpty() && !request.responseCallback)
            itr = m_requests.erase(itr); // no answer is required. clear data after send
        else
            ++itr;
    }
    if (sentBytes > 0) {
        qreal delaySecs = (qreal) sentBytes * 8.0 / SEND_BITRATE;
        queueNextSendData(int(delaySecs / 1000.0 + 0.5));
    }
}

void Transport::queueNextSendData(int delay)
{
    if (!m_nextSendQueued) {
        m_nextSendQueued = true;
        QTimer::singleShot(delay, this, SLOT(sendNextData()));
    }
}

}
