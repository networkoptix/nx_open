// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


#include "transaction_transport_base.h"

#include <atomic>

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>

#include <nx/network/http/base64_decoder_filter.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/byte_stream/custom_output_stream.h>
#include <nx/utils/byte_stream/sized_data_decoder.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/gzip/gzip_uncompressor.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/to_string.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/system_error.h>

#include <nx/cloud/db/api/ec2_request_paths.h>
#include <nx/network/cloud/cloud_connect_controller.h>

#include <transaction/json_transaction_serializer.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <transaction/transaction_transport_header.h>

#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/api/protocol_version.h>

//#define USE_SINGLE_TWO_WAY_CONNECTION
//!if not defined, ubjson is used
//#define USE_JSON
#ifndef USE_JSON
#define ENCODE_TO_BASE64
#endif
#define AGGREGATE_TRANSACTIONS_BEFORE_SEND

using namespace nx::vms;

namespace {

/*!
    Real number of transactions posted to the QnTransactionMessageBus can be greater
    (all transactions that read with single read from socket)
*/
static const int MAX_TRANS_TO_POST_AT_A_TIME = 16;
static const int kMinServerVersionWithClientKeepAlive = 3031;

} // namespace

namespace ec2 {

namespace ConnectionType {

const char* toString(Type val)
{
    switch (val)
    {
        case incoming:
            return "incoming";
        case outgoing:
            return "outgoing";
        case bidirectional:
            return "bidirectional";
        default:
            return "none";
    }
}

Type fromString(const std::string_view& str)
{
    if (str == "incoming")
        return incoming;
    else if (str == "outgoing")
        return outgoing;
    else if (str == "bidirectional")
        return bidirectional;
    else
        return none;
}

} // namespace ConnectionType

static const int DEFAULT_READ_BUFFER_SIZE = 4 * 1024;
static const int SOCKET_TIMEOUT = 1000 * 1000;
//following value is for VERY slow networks and VERY large transactions (e.g., some large image)
    //Connection keep-alive timeout is not influenced by this value
static const std::chrono::minutes kSocketSendTimeout(23);
const char* const QnTransactionTransportBase::TUNNEL_MULTIPART_BOUNDARY = "ec2boundary";
const char* const QnTransactionTransportBase::TUNNEL_CONTENT_TYPE = "multipart/mixed; boundary=ec2boundary";

QnTransactionTransportBase::QnTransactionTransportBase(
    const QnUuid& localSystemId,
    ConnectionGuardSharedState* const connectionGuardSharedState,
    const api::PeerData& localPeer,
    PeerRole peerRole,
    std::chrono::milliseconds tcpKeepAliveTimeout,
    int keepAliveProbeCount,
    nx::network::aio::AbstractAioThread* aioThread)
:
    m_localSystemId(localSystemId),
    m_localPeer(localPeer),
    m_peerRole(peerRole),
    m_connectionGuardSharedState(connectionGuardSharedState),
    m_tcpKeepAliveTimeout(tcpKeepAliveTimeout),
    m_keepAliveProbeCount(keepAliveProbeCount),
    m_idleConnectionTimeout(tcpKeepAliveTimeout * keepAliveProbeCount),
    m_timer(std::make_unique<nx::network::aio::Timer>()),
    m_remotePeerEcProtoVersion(nx::vms::api::kInitialProtocolVersion),
    m_localPeerProtocolVersion(nx::vms::api::protocolVersion())
{
    bindToAioThread(aioThread ? aioThread : getAioThread());

    m_lastConnectTime = 0;
    m_readSync = false;
    m_writeSync = false;
    m_syncDone = false;
    m_syncInProgress = false;
    m_needResync = false;
    m_state = NotDefined;
    m_connected = false;
    m_prevGivenHandlerID = 0;
    m_credentialsSource = CredentialsSource::serverKey;
    m_asyncReadScheduled = false;
    m_remoteIdentityTime = 0;
    m_connectionType = ConnectionType::none;
    m_compressResponseMsgBody = false;
    m_authOutgoingConnectionByServerKey = true;
    m_base64EncodeOutgoingTransactions = false;
    m_sentTranSequence = 0;
    m_waiterCount = 0;
    m_remotePeerSupportsKeepAlive = false;
    m_isKeepAliveEnabled = true;
}

QnTransactionTransportBase::QnTransactionTransportBase(
    const QnUuid& localSystemId,
    const std::string& connectionGuid,
    ConnectionLockGuard connectionLockGuard,
    const api::PeerData& localPeer,
    const api::PeerData& remotePeer,
    ConnectionType::Type connectionType,
    const nx::network::http::Request& request,
    const QByteArray& contentEncoding,
    std::chrono::milliseconds tcpKeepAliveTimeout,
    int keepAliveProbeCount,
    nx::network::aio::AbstractAioThread* aioThread)
:
    QnTransactionTransportBase(
        localSystemId,
        nullptr,
        localPeer,
        prAccepting,
        tcpKeepAliveTimeout,
        keepAliveProbeCount,
        aioThread)
{
    m_remotePeer = remotePeer;
    m_connectionType = connectionType;
    m_contentEncoding = contentEncoding;
    m_connectionGuid = connectionGuid;
    m_connectionLockGuard = std::make_unique<ConnectionLockGuard>(
        std::move(connectionLockGuard));

    nx::network::http::HttpHeaders::const_iterator ec2ProtoVersionIter =
        request.headers.find(Qn::EC2_PROTO_VERSION_HEADER_NAME);

    m_remotePeerEcProtoVersion =
        ec2ProtoVersionIter == request.headers.end()
        ? nx::vms::api::kInitialProtocolVersion
        : nx::utils::stoi(ec2ProtoVersionIter->second);

    //TODO #akolesnikov use binary filter stream for serializing transactions
    m_base64EncodeOutgoingTransactions = nx::network::http::getHeaderValue(
        request.headers, Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME ) == "true";

    auto keepAliveHeaderIter = request.headers.find(Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME);
    if (keepAliveHeaderIter != request.headers.end())
    {
        m_remotePeerSupportsKeepAlive = true;
        nx::network::http::header::KeepAlive keepAliveHeader;
        if (keepAliveHeader.parse(keepAliveHeaderIter->second))
            m_tcpKeepAliveTimeout = std::max(
                std::chrono::duration_cast<std::chrono::seconds>(m_tcpKeepAliveTimeout),
                keepAliveHeader.timeout);
    }

    m_readBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_lastReceiveTimer.invalidate();

    using namespace std::placeholders;
    if( m_contentEncoding == "gzip" )
        m_compressResponseMsgBody = true;

    //creating parser sequence: http_msg_stream_parser -> ext_headers_processor -> transaction handler
    auto incomingTransactionsRequestsParser =
        std::make_shared<nx::network::http::HttpMessageStreamParser>();
    std::weak_ptr<nx::network::http::HttpMessageStreamParser>
        incomingTransactionsRequestsParserWeak(incomingTransactionsRequestsParser );

    // This filter receives single HTTP message.
    auto extensionHeadersProcessor = nx::utils::bstream::makeFilterWithFunc(
        [this, incomingTransactionsRequestsParserWeak]() {
            if (auto incomingTransactionsRequestsParserStrong = incomingTransactionsRequestsParserWeak.lock())
                processChunkExtensions(incomingTransactionsRequestsParserStrong->currentMessage().headers());
        });

    extensionHeadersProcessor->setNextFilter(
        nx::utils::bstream::makeCustomOutputStream(
            [this](auto&&... args) { receivedTransactionNonSafe(std::move(args)...); }));

    incomingTransactionsRequestsParser->setNextFilter(std::move(extensionHeadersProcessor));

    m_incomingTransactionStreamParser = std::move(incomingTransactionsRequestsParser);

    QUrlQuery urlQuery(request.requestLine.url.query());
    const auto& queryItems = urlQuery.queryItems();
    std::transform(
        queryItems.begin(), queryItems.end(),
        std::inserter(m_httpQueryParams, m_httpQueryParams.end()),
        [](const auto& queryItem) { return std::make_pair(queryItem.first, queryItem.second); });
}

QnTransactionTransportBase::QnTransactionTransportBase(
    const QnUuid& localSystemId,
    ConnectionGuardSharedState* const connectionGuardSharedState,
    const api::PeerData& localPeer,
    std::chrono::milliseconds tcpKeepAliveTimeout,
    int keepAliveProbeCount,
    nx::network::aio::AbstractAioThread* aioThread)
:
    QnTransactionTransportBase(
        localSystemId,
        connectionGuardSharedState,
        localPeer,
        prOriginating,
        tcpKeepAliveTimeout,
        keepAliveProbeCount,
        aioThread)
{
    m_connectionType =
#ifdef USE_SINGLE_TWO_WAY_CONNECTION
#error "Bidirection mode is not supported any more due to unique_ptr for socket. Need refactor to support it."
        ConnectionType::bidirectional
#else
        ConnectionType::incoming
#endif
        ;
    m_connectionGuid = QnUuid::createUuid().toSimpleString().toStdString();
#ifdef ENCODE_TO_BASE64
    m_base64EncodeOutgoingTransactions = true;
#endif

    m_readBuffer.reserve(DEFAULT_READ_BUFFER_SIZE);
    m_lastReceiveTimer.invalidate();

    NX_VERBOSE(this, "Constructor");

    //creating parser sequence: multipart_parser -> ext_headers_processor -> transaction handler
    m_multipartContentParser = std::make_shared<nx::network::http::MultipartContentParser>();
    std::weak_ptr<nx::network::http::MultipartContentParser> multipartContentParserWeak(
        m_multipartContentParser);
    auto extensionHeadersProcessor = nx::utils::bstream::makeFilterWithFunc( //this filter receives single multipart message
        [this, multipartContentParserWeak]()
        {
            if (auto multipartContentParser = multipartContentParserWeak.lock())
                processChunkExtensions(multipartContentParser->prevFrameHeaders());
        });
    extensionHeadersProcessor->setNextFilter(
        nx::utils::bstream::makeCustomOutputStream(
            [this](auto&&... args) { receivedTransactionNonSafe(std::move(args)...); }));
    m_multipartContentParser->setNextFilter(std::move(extensionHeadersProcessor));

    m_incomingTransactionStreamParser = m_multipartContentParser;
}

QnTransactionTransportBase::~QnTransactionTransportBase()
{
    NX_VERBOSE(this, "Destructor");

    stopWhileInAioThread();

    // TODO: #akolesnikov: move following logic to ::ec2::QnTransactionTransport
    // since it can result in deadlock if used in aio thread.
    // Though, currently cloud_db does not use following logic.
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_state = Closed;
        m_cond.wakeAll();   //signalling waiters that connection is being closed
        //waiting for waiters to quit
        while (m_waiterCount > 0)
            m_cond.wait(lk.mutex());
    }
}

void QnTransactionTransportBase::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    nx::network::aio::BasicPollable::bindToAioThread(aioThread);

    m_timer->bindToAioThread(aioThread);
    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);
    if (m_outgoingTranClient)
        m_outgoingTranClient->bindToAioThread(aioThread);
    if (m_outgoingDataSocket)
        m_outgoingDataSocket->bindToAioThread(aioThread);
    if (m_incomingDataSocket)
        m_incomingDataSocket->bindToAioThread(aioThread);
}

void QnTransactionTransportBase::stopWhileInAioThread()
{
    m_timer.reset();
    m_httpClient.reset();
    m_outgoingTranClient.reset();
    m_outgoingDataSocket.reset();
    m_incomingDataSocket.reset();
}

void QnTransactionTransportBase::setReceivedTransactionsQueueControlEnabled(bool value)
{
    m_receivedTransactionsQueueControlEnabled = value;
}

void QnTransactionTransportBase::setLocalPeerProtocolVersion(int version)
{
    m_localPeerProtocolVersion = version;
}

void QnTransactionTransportBase::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
}

void QnTransactionTransportBase::setOutgoingConnection(
    std::unique_ptr<nx::network::AbstractCommunicatingSocket> socket)
{
    using namespace std::chrono;

    m_outgoingDataSocket = std::move(socket);
    m_outgoingDataSocket->bindToAioThread(getAioThread());
    if (!m_outgoingDataSocket->setSendTimeout(
            duration_cast<milliseconds>(kSocketSendTimeout).count()))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_DEBUG(this,
            "Error setting socket write timeout for transaction connection %1 received from %2: %3",
            m_connectionGuid,
            m_outgoingDataSocket->getForeignAddress().toString(),
            SystemError::toString(osErrorCode));
    }

    if (m_connectionType == ConnectionType::bidirectional)
    {
        NX_CRITICAL(0, "Bidirection mode is not supported any more");
        //m_incomingDataSocket = m_outgoingDataSocket;
    }
}

void QnTransactionTransportBase::monitorConnectionForClosure()
{
    startSendKeepAliveTimerNonSafe();

    m_dummyReadBuffer.reserve(DEFAULT_READ_BUFFER_SIZE);
    if (!m_outgoingDataSocket->setNonBlockingMode(true))
    {
        return m_outgoingDataSocket->post(
            [this, error = SystemError::getLastOSErrorCode()]()
            {
                onMonitorConnectionForClosure(error, (size_t) -1);
            });
    }

    m_outgoingDataSocket->readSomeAsync(
        &m_dummyReadBuffer,
        [this](auto&&... args) { onMonitorConnectionForClosure(std::move(args)...); });
}

std::chrono::milliseconds QnTransactionTransportBase::connectionKeepAliveTimeout() const
{
    return m_tcpKeepAliveTimeout;
}

int QnTransactionTransportBase::keepAliveProbeCount() const
{
    return m_keepAliveProbeCount;
}

void QnTransactionTransportBase::addDataToTheSendQueue(nx::Buffer data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_base64EncodeOutgoingTransactions)
    {
        //adding size before transaction data
        const uint32_t dataSize = htonl(data.size());
        nx::Buffer dataWithSize;
        dataWithSize.resize(sizeof(dataSize) + data.size());
        // TODO #akolesnikov too many memcopy here. Should use stream base64 encoder to write directly
        // to the output buffer.
        memcpy(dataWithSize.data(), &dataSize, sizeof(dataSize));
        memcpy(
            dataWithSize.data() + sizeof(dataSize),
            data.data(),
            data.size());
        data.clear();
        m_dataToSend.push_back(std::move(dataWithSize));
    }
    else
    {
        m_dataToSend.push_back(std::move(data));
    }

    if (m_dataToSend.size() == 1)
        serializeAndSendNextDataBuffer();
}

void QnTransactionTransportBase::setPostTranUrl(const nx::utils::Url& url)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_mockedUpPostTranUrl = url;
}

void QnTransactionTransportBase::setState(State state)
{
    NX_VERBOSE(this, nx::format("State changed to %1 from outside").args(toString(state)));

    NX_MUTEX_LOCKER lock(&m_mutex);
    setStateNoLock(state);
}

void QnTransactionTransportBase::processExtraData()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!m_extraData.empty())
    {
        processTransactionData(m_extraData);
        m_extraData.clear();
    }
}

void QnTransactionTransportBase::startListening()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    startListeningNonSafe();
}

void QnTransactionTransportBase::setStateNoLock(State state)
{
    if (state == Connected)
        m_connected = true;

    if (m_state == Error && state != Closed)
    {
        ; // only Error -> Closed setState is allowed
    }
    else if (m_state == Closed)
    {
        ; // no state allowed after Closed
    }
    else if (this->m_state != state)
    {
        this->m_state = state;

        // TODO: #akolesnikov: if stateChanged handler frees this object then m_mutex is destroyed locked
        emit stateChanged(state);
    }
    m_cond.wakeAll();
}

const api::PeerData& QnTransactionTransportBase::localPeer() const
{
    return m_localPeer;
}

const api::PeerData& QnTransactionTransportBase::remotePeer() const
{
    return m_remotePeer;
}

nx::utils::Url QnTransactionTransportBase::remoteAddr() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    // Emulating deep copy here
    nx::utils::Url tmpUrl(m_remoteAddr);
    tmpUrl.setUserName(tmpUrl.userName());
    return tmpUrl;
}

nx::network::SocketAddress QnTransactionTransportBase::remoteSocketAddr() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return nx::network::SocketAddress(
        m_remoteAddr.host().toStdString(),
        m_remoteAddr.port());
}

int QnTransactionTransportBase::remotePeerProtocolVersion() const
{
    return m_remotePeerEcProtoVersion;
}

std::multimap<QString, QString> QnTransactionTransportBase::httpQueryParams() const
{
    return m_httpQueryParams;
}

QnTransactionTransportBase::State QnTransactionTransportBase::getState() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_state;
}

bool QnTransactionTransportBase::isIncoming() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_peerRole == prAccepting;
}

int QnTransactionTransportBase::setHttpChunkExtensonHandler(
    HttpChunkExtensonHandler eventHandler)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_httpChunkExtensonHandlers.emplace(++m_prevGivenHandlerID, std::move(eventHandler));
    return m_prevGivenHandlerID;
}

int QnTransactionTransportBase::setBeforeSendingChunkHandler(
    BeforeSendingChunkHandler eventHandler)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_beforeSendingChunkHandlers.emplace(++m_prevGivenHandlerID, std::move(eventHandler));
    return m_prevGivenHandlerID;
}

void QnTransactionTransportBase::removeEventHandler(int eventHandlerID)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_httpChunkExtensonHandlers.erase(eventHandlerID);
    m_beforeSendingChunkHandlers.erase(eventHandlerID);
}

void QnTransactionTransportBase::doOutgoingConnect(
    const nx::utils::Url& remotePeerUrl)
{
    NX_VERBOSE(this,
        "doOutgoingConnect. remotePeerUrl = %1", remotePeerUrl);

    setState(ConnectingStage1);

    m_httpClient =
        nx::network::http::AsyncHttpClient::create(nx::network::ssl::kDefaultCertificateCheck);
    m_httpClient->bindToAioThread(getAioThread());
    m_httpClient->setSendTimeoutMs(m_idleConnectionTimeout.count());
    m_httpClient->setResponseReadTimeoutMs(m_idleConnectionTimeout.count());
    connect(
        m_httpClient.get(), &nx::network::http::AsyncHttpClient::responseReceived,
        this, &QnTransactionTransportBase::at_responseReceived,
        Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        this, &QnTransactionTransportBase::at_httpClientDone,
        Qt::DirectConnection);

    if (remotePeerUrl.userName().isEmpty())
    {
        fillAuthInfo( m_httpClient, m_credentialsSource == CredentialsSource::serverKey );
    }
    else
    {
        m_credentialsSource = CredentialsSource::remoteUrl;
        m_httpClient->setCredentials(nx::network::http::PasswordCredentials(
            remotePeerUrl.userName().toStdString(), remotePeerUrl.password().toStdString()));
    }

    if (m_localPeer.isServer())
    {
        m_httpClient->addAdditionalHeader(
            Qn::EC2_SYSTEM_ID_HEADER_NAME,
            m_localSystemId.toStdString());
    }
    if (m_base64EncodeOutgoingTransactions)    //requesting server to encode transactions
    {
        m_httpClient->addAdditionalHeader(
            Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME,
            "true");
    }
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME,
        nx::network::http::header::KeepAlive(
            std::chrono::duration_cast<std::chrono::seconds>(
                m_tcpKeepAliveTimeout)).toString());

    QUrlQuery q;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_remoteAddr = remotePeerUrl;
        if (!m_remoteAddr.userName().isEmpty())
        {
            m_remoteAddr.setUserName(QString());
            m_remoteAddr.setPassword(QString());
        }
        q = QUrlQuery(m_remoteAddr.query());
    }

    q.addQueryItem("format", nx::toString(m_localPeer.dataFormat));

    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_GUID_HEADER_NAME,
        m_connectionGuid.c_str());
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME,
        ConnectionType::toString(m_connectionType));   //incoming means this peer wants to receive data via this connection
    m_httpClient->addAdditionalHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        m_localPeer.instanceId.toStdString());
    m_httpClient->addAdditionalHeader(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        std::to_string(m_localPeerProtocolVersion));
    if (m_userAgent)
        m_httpClient->addAdditionalHeader("User-Agent", *m_userAgent);

    q.addQueryItem("peerType", nx::toString(m_localPeer.peerType));

    // add deprecated query item 'isClient' for compatibility with old server version <= 2.6
    if (m_localPeer.peerType == api::PeerType::mobileClient)
        q.addQueryItem("isClient", QString());

    // Client reconnects to the server
    if (m_localPeer.isClient())
    {
        setState(ConnectingStage2); // one GET method for client peer is enough
        setReadSync(true);
    }

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_remoteAddr.setQuery(q);
    }

    m_httpClient->removeAdditionalHeader(Qn::EC2_CONNECTION_STATE_HEADER_NAME);
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_STATE_HEADER_NAME,
        toString(getState()));

    nx::utils::Url url = remoteAddr();
    url.setPath(url.path() + lit("/") + toString(getState()));
    m_httpClient->setAuthType(url.scheme() == nx::network::http::kSecureUrlSchemeName
        ? nx::network::http::AuthType::authBasicAndDigest
        : nx::network::http::AuthType::authDigest);
    m_httpClient->doGet(url);
}

void QnTransactionTransportBase::markAsNotSynchronized()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_readSync = false;
    m_writeSync = false;
}

void QnTransactionTransportBase::repeatDoGet()
{
    m_httpClient->removeAdditionalHeader(
        Qn::EC2_CONNECTION_STATE_HEADER_NAME);
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_STATE_HEADER_NAME, toString(getState()));

    nx::utils::Url url = remoteAddr();
    url.setPath(url.path() + lit("/") + toString(getState()));
    m_httpClient->doGet(url);
}

void QnTransactionTransportBase::cancelConnecting()
{
    NX_DEBUG(this,
        "Connection to peer %1 canceled from state %2",
        m_remotePeer.id.toString(), toString(getState()));
    setState(Error);
}

void QnTransactionTransportBase::onSomeBytesRead(
    SystemError::ErrorCode errorCode, size_t bytesRead)
{
    NX_VERBOSE(this,
        "onSomeBytesRead. errorCode = %1, bytesRead = %2",
        SystemError::toString(errorCode), bytesRead);

    emit onSomeDataReceivedFromRemotePeer();

    NX_MUTEX_LOCKER lock(&m_mutex);

    m_asyncReadScheduled = false;
    m_lastReceiveTimer.invalidate();

    if (errorCode || bytesRead == 0)   //error or connection closed
    {
        if (errorCode == SystemError::timedOut)
        {
            NX_DEBUG(this, "Peer %1 timed out. Disconnecting...",
                m_remotePeer.id.toString());
        }
        NX_VERBOSE(this, nx::format("Closing connection due to error %1").args(SystemError::toString(
            errorCode == SystemError::noError
            ? SystemError::connectionReset
            : errorCode)));
        return setStateNoLock(State::Error);
    }

    if (m_state >= QnTransactionTransportBase::Closed)
    {
        NX_VERBOSE(this,
            "Connection to %1 is closed. Not reading anymore",
            m_remotePeer.id.toString());
        return;
    }

    NX_ASSERT(m_state == ReadyForStreaming);

    //parsing and processing input data
    if (!m_incomingTransactionStreamParser->processData(m_readBuffer))
    {
        NX_DEBUG(this,
            "Error parsing data from peer %1. Disconnecting...", m_remotePeer.id.toString());
        return setStateNoLock(State::Error);
    }

    m_readBuffer.resize(0);

    if (m_receivedTransactionsQueueControlEnabled &&
        m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME)
    {
        NX_VERBOSE(this, nx::format("There are already %1 transactions posted. "
            "Suspending receiving new transactions until pending transactions are processed")
            .args(m_postedTranCount));
        return; //not reading futher while that much transactions are not processed yet
    }

    m_readBuffer.reserve(m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE);
    scheduleAsyncRead();
}

void QnTransactionTransportBase::receivedTransactionNonSafe(
    const nx::ConstBufferRefType& tranDataWithHeader)
{
    if (tranDataWithHeader.empty())
        return; //it happens in case of keep-alive message

    const auto& tranData = tranDataWithHeader;

    QByteArray serializedTran;
    QnTransactionTransportHeader transportHeader;

    switch (m_remotePeer.dataFormat)
    {
        case Qn::JsonFormat:
            if (!QnJsonTransactionSerializer::deserializeTran(
                    reinterpret_cast<const quint8*>(tranData.data()),
                    static_cast<int>(tranData.size()),
                    transportHeader,
                    serializedTran))
            {
                NX_ASSERT(false);
                NX_WARNING(this,
                    "Error deserializing JSON data from peer %1. Disconnecting...",
                    m_remotePeer.id.toString());
                setStateNoLock(State::Error);
                return;
            }
            break;

        case Qn::UbjsonFormat:
            if (!QnUbjsonTransactionSerializer::deserializeTran(
                    reinterpret_cast<const quint8*>(tranData.data()),
                    static_cast<int>(tranData.size()),
                    transportHeader,
                    serializedTran))
            {
                NX_ASSERT(false);
                NX_WARNING(this,
                    "Error deserializing Ubjson data from peer %1. Disconnecting...",
                    m_remotePeer.id.toString());
                setStateNoLock(State::Error);
                return;
            }
            break;

        default:
            NX_WARNING(this,
                "Received unkown format from peer %1. Disconnecting...",
                m_remotePeer.id.toString());
            setStateNoLock(State::Error);
            return;
    }

    if (!transportHeader.isNull())
    {
        NX_ASSERT(!transportHeader.processedPeers.empty());
        NX_DEBUG(this,
            "receivedTransactionNonSafe. Got transaction with seq %1 from %2",
            transportHeader.sequence, m_remotePeer.id.toString());
    }

    emit gotTransaction(
        m_remotePeer.dataFormat,
        std::move(serializedTran),
        transportHeader);

    if (m_receivedTransactionsQueueControlEnabled)
        ++m_postedTranCount;
}

bool QnTransactionTransportBase::hasUnsendData() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return !m_dataToSend.empty();
}

void QnTransactionTransportBase::receivedTransaction(
    const nx::network::http::HttpHeaders& headers,
    const nx::ConstBufferRefType& tranData)
{
    emit onSomeDataReceivedFromRemotePeer();

    NX_MUTEX_LOCKER lock(&m_mutex);

    processChunkExtensions(headers);

    if (nx::network::http::getHeaderValue(
            headers,
            Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME) == "true")
    {
        const auto& decodedTranData = nx::utils::fromBase64(tranData);
        //decodedTranData can contain multiple transactions
        if (!m_sizedDecoder)
        {
            m_sizedDecoder = std::make_shared<nx::utils::bstream::SizedDataDecodingFilter>();
            m_sizedDecoder->setNextFilter(nx::utils::bstream::makeCustomOutputStream(
                [this](auto&&... args) { receivedTransactionNonSafe(std::move(args)...); }));
        }
        if (!m_sizedDecoder->processData(decodedTranData))
        {
            NX_WARNING(this,
                "Error parsing data (2) from peer %1. Disconnecting...",
                m_remotePeer.id.toString());
            return setStateNoLock(State::Error);
        }
    }
    else
    {
        receivedTransactionNonSafe(tranData);
    }
}

void QnTransactionTransportBase::transactionProcessed()
{
    if (!m_receivedTransactionsQueueControlEnabled)
        return;

    post(
        [this]()
        {
            NX_MUTEX_LOCKER lock(&m_mutex);

            --m_postedTranCount;
            if (m_postedTranCount < MAX_TRANS_TO_POST_AT_A_TIME)
                m_cond.wakeAll();   //signalling waiters that we are ready for new transactions once again
            if (m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME ||     //not reading futher while that much transactions are not processed yet
                m_asyncReadScheduled ||      //async read is ongoing already, overlapping reads are not supported by sockets api
                m_state > ReadyForStreaming)
            {
                return;
            }

            NX_ASSERT(m_incomingDataSocket || m_outgoingDataSocket);

            m_readBuffer.reserve(m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE);
            scheduleAsyncRead();
        });
}

std::string QnTransactionTransportBase::connectionGuid() const
{
    return m_connectionGuid;
}

void QnTransactionTransportBase::setIncomingTransactionChannelSocket(
    std::unique_ptr<nx::network::AbstractCommunicatingSocket> socket,
    const nx::network::http::Request& /*request*/,
    const QByteArray& requestBuf)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    NX_ASSERT(m_peerRole == prAccepting);
    NX_ASSERT(m_connectionType != ConnectionType::bidirectional);

    m_incomingDataSocket = std::move(socket);
    m_incomingDataSocket->bindToAioThread(getAioThread());

    //checking transactions format
    if (!m_incomingTransactionStreamParser->processData(nx::toBufferView(requestBuf)))
    {
        NX_WARNING(this,
            "Error parsing incoming data (3) from peer %1. Disconnecting...",
            m_remotePeer.id.toString());
        return setStateNoLock(State::Error);
    }

    startListeningNonSafe();
}

void QnTransactionTransportBase::lock()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    ++m_waiterCount;
}

void QnTransactionTransportBase::unlock()
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    --m_waiterCount;
    m_cond.wakeAll();    //signalling that we are not waiting anymore
}

void QnTransactionTransportBase::waitForNewTransactionsReady()
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    auto postedTranQueueIsReady =
        [this]()
        {
            return !m_receivedTransactionsQueueControlEnabled ||
                m_postedTranCount < MAX_TRANS_TO_POST_AT_A_TIME;
        };

    if (postedTranQueueIsReady() && m_state >= ReadyForStreaming)
        return;

    //waiting for some transactions to be processed
    while ((!postedTranQueueIsReady() && m_state != Closed) || m_state < ReadyForStreaming)
    {
        m_cond.wait(lk.mutex());
    }
}

void QnTransactionTransportBase::connectionFailure()
{
    NX_DEBUG(this,
        "Connection to peer %1 failure. Disconnecting...", m_remotePeer.id.toString());
    setState(Error);
}

void QnTransactionTransportBase::setKeepAliveEnabled(bool value)
{
    m_isKeepAliveEnabled = value;
}

void QnTransactionTransportBase::sendHttpKeepAlive()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_dataToSend.empty())
    {
        m_dataToSend.push_back( nx::Buffer() );
        serializeAndSendNextDataBuffer();
    }

    startSendKeepAliveTimerNonSafe();
}

void QnTransactionTransportBase::startSendKeepAliveTimerNonSafe()
{
    if (!m_isKeepAliveEnabled)
        return;

    if (m_peerRole == prAccepting)
    {
        NX_ASSERT(m_outgoingDataSocket);
        m_outgoingDataSocket->registerTimer(
            m_tcpKeepAliveTimeout.count(),
            std::bind(&QnTransactionTransportBase::sendHttpKeepAlive, this));
    }
    else
    {
        //we using http client to send transactions
        m_timer->cancelSync();
        m_timer->start(
            m_tcpKeepAliveTimeout,
            std::bind(&QnTransactionTransportBase::sendHttpKeepAlive, this));
    }
}

void QnTransactionTransportBase::onMonitorConnectionForClosure(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (errorCode != SystemError::noError && errorCode != SystemError::timedOut)
    {
        NX_DEBUG(this,
            "transaction connection %1 received from %2 failed: %3",
            m_connectionGuid, m_outgoingDataSocket->getForeignAddress().toString(),
            SystemError::toString(errorCode));
        return setStateNoLock(State::Error);
    }

    if (bytesRead == 0)
    {
        NX_DEBUG(this,
            "transaction connection %1 received from %2 has been closed by remote peer",
            m_connectionGuid, m_outgoingDataSocket->getForeignAddress().toString());
        return setStateNoLock(State::Error);
    }

    //TODO #akolesnikov should read HTTP responses here and check result code

    using namespace std::placeholders;
    m_dummyReadBuffer.resize(0);
    m_outgoingDataSocket->readSomeAsync(
        &m_dummyReadBuffer,
        [this](auto&&... args) { onMonitorConnectionForClosure(std::move(args)...); });
}

nx::utils::Url QnTransactionTransportBase::generatePostTranUrl()
{
    nx::utils::Url postTranUrl = m_postTranBaseUrl;
    postTranUrl.setPath(nx::format("%1/%2").args(postTranUrl.path(), ++m_sentTranSequence));
    return postTranUrl;
}

void QnTransactionTransportBase::aggregateOutgoingTransactionsNonSafe()
{
    static const int MAX_AGGREGATED_TRAN_SIZE_BYTES = 128 * 1024;
    //std::deque<DataToSend> m_dataToSend;
    //searching first transaction not being sent currently
    auto saveToIter = std::find_if(
        m_dataToSend.begin(),
        m_dataToSend.end(),
        [](const DataToSend& data)->bool { return data.encodedSourceData.empty(); });
    if (std::distance(saveToIter, m_dataToSend.end()) < 2)
        return; //nothing to aggregate

    //aggregating. Transaction data already contains size
    auto it = std::next(saveToIter);
    for (;
        it != m_dataToSend.end();
        ++it)
    {
        if (saveToIter->sourceData.size() + it->sourceData.size() > MAX_AGGREGATED_TRAN_SIZE_BYTES)
            break;

        saveToIter->sourceData += it->sourceData;
        it->sourceData.clear();
    }
    //erasing aggregated transactions
    m_dataToSend.erase(std::next(saveToIter), it);
}

bool QnTransactionTransportBase::remotePeerSupportsKeepAlive() const
{
    return m_remotePeerSupportsKeepAlive;
}

bool QnTransactionTransportBase::isHttpKeepAliveTimeout() const
{
    NX_MUTEX_LOCKER lock( &m_mutex );
    return (m_lastReceiveTimer.isValid() &&  //if not valid we still have not begun receiving transactions
         (m_lastReceiveTimer.elapsed() > m_idleConnectionTimeout.count()));
}

void QnTransactionTransportBase::serializeAndSendNextDataBuffer()
{
    NX_ASSERT(!m_dataToSend.empty());

#ifdef AGGREGATE_TRANSACTIONS_BEFORE_SEND
    if (m_base64EncodeOutgoingTransactions)
        aggregateOutgoingTransactionsNonSafe();
#endif  //AGGREGATE_TRANSACTIONS_BEFORE_SEND

    DataToSend& dataCtx = m_dataToSend.front();

    if (m_base64EncodeOutgoingTransactions)
        dataCtx.sourceData = nx::utils::toBase64(dataCtx.sourceData); //TODO #akolesnikov should use streaming base64 encoder in addDataToTheSendQueue method

    if (dataCtx.encodedSourceData.empty())
    {
        if (m_peerRole == prAccepting)
        {
            //sending transactions as a response to GET request
            nx::network::http::HttpHeaders headers;
            headers.emplace(
                "Content-Type",
                m_base64EncodeOutgoingTransactions
                ? "application/text"
                : Qn::serializationFormatToHttpContentType(m_remotePeer.dataFormat));
            headers.emplace("Content-Length", std::to_string(dataCtx.sourceData.size()));
            addHttpChunkExtensions(&headers);

            dataCtx.encodedSourceData.clear();
            nx::utils::buildString(&dataCtx.encodedSourceData, "\r\n--", TUNNEL_MULTIPART_BOUNDARY, "\r\n");
            nx::network::http::serializeHeaders(headers, &dataCtx.encodedSourceData);
            dataCtx.encodedSourceData += "\r\n";
            dataCtx.encodedSourceData += dataCtx.sourceData;

            if (m_compressResponseMsgBody)
            {
                //encoding outgoing message body
                dataCtx.encodedSourceData = nx::utils::bstream::gzip::Compressor::compressData(dataCtx.encodedSourceData);
            }
        }
        else    //m_peerRole == prOriginating
        {
            NX_ASSERT(!m_outgoingDataSocket);
            dataCtx.encodedSourceData = dataCtx.sourceData;
        }
    }

    NX_VERBOSE(this, "Sending data buffer (%1 bytes) to the peer %2",
        dataCtx.encodedSourceData.size(), m_remotePeer.id.toString());

    if (m_outgoingDataSocket)
    {
        m_outgoingDataSocket->sendAsync(
            &dataCtx.encodedSourceData,
            [this](auto&&... args) { onDataSent(std::move(args)...); });
    }
    else  //m_peerRole == prOriginating
    {
        NX_ASSERT(m_peerRole == prOriginating && m_connectionType != ConnectionType::bidirectional);

        //using http client just to authenticate on server
        if (!m_outgoingTranClient)
        {
            using namespace std::chrono;
            m_outgoingTranClient = nx::network::http::AsyncHttpClient::create(
                nx::network::ssl::kDefaultCertificateCheck);
            m_outgoingTranClient->bindToAioThread(getAioThread());
            m_outgoingTranClient->setSendTimeoutMs(
                duration_cast<milliseconds>(kSocketSendTimeout).count());   //it can take a long time to send large transactions
            m_outgoingTranClient->setResponseReadTimeoutMs(m_idleConnectionTimeout.count());
            m_outgoingTranClient->addAdditionalHeader(
                Qn::EC2_CONNECTION_GUID_HEADER_NAME,
                m_connectionGuid.c_str());
            m_outgoingTranClient->addAdditionalHeader(
                Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME,
                ConnectionType::toString(ConnectionType::outgoing));
            if (m_base64EncodeOutgoingTransactions)    //informing server that transaction is encoded
                m_outgoingTranClient->addAdditionalHeader(
                    Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME,
                    "true");
            connect(
                m_outgoingTranClient.get(), &nx::network::http::AsyncHttpClient::done,
                this, &QnTransactionTransportBase::postTransactionDone,
                Qt::DirectConnection);

            if (m_remotePeerCredentials.isNull())
            {
                fillAuthInfo(m_outgoingTranClient, true);
            }
            else
            {
                m_outgoingTranClient->setCredentials(m_remotePeerCredentials);
            }

            if (m_mockedUpPostTranUrl)
            {
                m_postTranBaseUrl = *m_mockedUpPostTranUrl;
            }
            else
            {
                m_postTranBaseUrl = m_remoteAddr;
                if (m_remotePeer.peerType == api::PeerType::cloudServer)
                    m_postTranBaseUrl.setPath(QString(nx::cloud::db::api::kPushEc2TransactionPath));
                else
                    m_postTranBaseUrl.setPath(lit("/ec2/forward_events"));
                m_postTranBaseUrl.setQuery(QString());
            }
        }

        nx::network::http::HttpHeaders additionalHeaders;
        addHttpChunkExtensions(&additionalHeaders);
        for (const auto& header: additionalHeaders)
        {
            //removing prev header value (if any)
            m_outgoingTranClient->removeAdditionalHeader(header.first);
            m_outgoingTranClient->addAdditionalHeader(header.first, header.second);
        }

        const auto url = generatePostTranUrl();
        m_outgoingTranClient->setAuthType(url.scheme() == nx::network::http::kSecureUrlSchemeName
            ? nx::network::http::AuthType::authBasicAndDigest
            : nx::network::http::AuthType::authDigest);
        m_outgoingTranClient->doPost(url,
            m_base64EncodeOutgoingTransactions
                ? "application/text"
                : Qn::serializationFormatToHttpContentType(m_remotePeer.dataFormat),
            dataCtx.encodedSourceData);
    }
}

void QnTransactionTransportBase::onDataSent(
    SystemError::ErrorCode errorCode,
    size_t bytesSent)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    if (errorCode)
    {
        NX_DEBUG(this, "Failed to send %1 bytes to %2. %3",
            m_dataToSend.front().encodedSourceData.size(),
            m_remotePeer.id.toString(), SystemError::toString(errorCode));
        m_dataToSend.pop_front();
        return setStateNoLock(State::Error);
    }
    NX_ASSERT(bytesSent == (size_t)m_dataToSend.front().encodedSourceData.size());

    m_dataToSend.pop_front();
    if (m_dataToSend.empty())
        return;

    serializeAndSendNextDataBuffer();
}

void QnTransactionTransportBase::at_responseReceived(
    const nx::network::http::AsyncHttpClientPtr& client)
{
    const int statusCode = client->response()->statusLine.statusCode;

    NX_VERBOSE(this,
        "at_responseReceived. statusCode = %1", statusCode);

    if (statusCode == nx::network::http::StatusCode::unauthorized)
    {
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        if (m_credentialsSource < CredentialsSource::none)
        {
            fillAuthInfo(m_httpClient, m_credentialsSource == CredentialsSource::serverKey);
            repeatDoGet();
        }
        else
        {
            QnUuid guid(nx::network::http::getHeaderValue(client->response()->headers, Qn::EC2_SERVER_GUID_HEADER_NAME));
            if (!guid.isNull())
            {
                emit peerIdDiscovered(remoteAddr(), guid);
                emit remotePeerUnauthorized(guid);
            }
            cancelConnecting();
        }
        return;
    }

    nx::network::http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find(Qn::EC2_GUID_HEADER_NAME);
    nx::network::http::HttpHeaders::const_iterator itrRuntimeGuid = client->response()->headers.find(Qn::EC2_RUNTIME_GUID_HEADER_NAME);
    nx::network::http::HttpHeaders::const_iterator itrSystemIdentityTime = client->response()->headers.find(Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME);
    if (itrSystemIdentityTime != client->response()->headers.end())
        setRemoteIdentityTime(nx::utils::stoll(itrSystemIdentityTime->second));

    if (itrGuid == client->response()->headers.end())
    {
        cancelConnecting();
        return;
    }

    nx::network::http::HttpHeaders::const_iterator ec2CloudHostItr =
        client->response()->headers.find(Qn::EC2_CLOUD_HOST_HEADER_NAME);

    nx::network::http::HttpHeaders::const_iterator ec2ProtoVersionIter =
        client->response()->headers.find(Qn::EC2_PROTO_VERSION_HEADER_NAME);

    m_remotePeerEcProtoVersion =
        ec2ProtoVersionIter == client->response()->headers.end()
        ? nx::vms::api::kInitialProtocolVersion
        : nx::utils::stoi(ec2ProtoVersionIter->second);

    if (!m_localPeer.isMobileClient())
    {
        //checking remote server protocol version
        if (m_localPeerProtocolVersion != m_remotePeerEcProtoVersion)
        {
            NX_WARNING(this, nx::format("Cannot connect to server %1 because of different EC2 proto version. "
                "Local peer version: %2, remote peer version: %3")
                .args(client->url(), m_localPeerProtocolVersion, m_remotePeerEcProtoVersion));
            cancelConnecting();
            return;
        }

        const auto remotePeerCloudHost = ec2CloudHostItr == client->response()->headers.end()
            ? nx::network::SocketGlobals::cloud().cloudHost()
            : ec2CloudHostItr->second;

        if (nx::network::SocketGlobals::cloud().cloudHost() != remotePeerCloudHost)
        {
            NX_WARNING(this, nx::format("Cannot connect to server %1 because they have different built in cloud host setting. "
                "Local peer host: %2, remote peer host: %3").
                arg(client->url()).arg(nx::network::SocketGlobals::cloud().cloudHost()).arg(remotePeerCloudHost));
            cancelConnecting();
            return;
        }
    }

    m_remotePeer.id = QnUuid(itrGuid->second);
    if (itrRuntimeGuid != client->response()->headers.end())
        m_remotePeer.instanceId = QnUuid(itrRuntimeGuid->second);

    NX_ASSERT(!m_remotePeer.instanceId.isNull());
    if (m_remotePeer.id == kCloudPeerId)
        m_remotePeer.peerType = api::PeerType::cloudServer;
    else if (ec2CloudHostItr == client->response()->headers.end())
        m_remotePeer.peerType = api::PeerType::oldServer; // outgoing connections for server or cloud peers only
    else
        m_remotePeer.peerType = api::PeerType::server; // outgoing connections for server or cloud peers only
    m_remotePeer.dataFormat = m_localPeer.dataFormat;

    emit peerIdDiscovered(remoteAddr(), m_remotePeer.id);

    if (!m_connectionLockGuard)
    {
        NX_CRITICAL(m_connectionGuardSharedState);
        m_connectionLockGuard = std::make_unique<ConnectionLockGuard>(
            m_localPeer.id,
            m_connectionGuardSharedState,
            m_remotePeer.id,
            ConnectionLockGuard::Direction::Outgoing);
    }

    if (!nx::network::http::StatusCode::isSuccessCode(statusCode)) //checking that statusCode is 2xx
    {
        cancelConnecting();
        return;
    }

    if (client->credentials().authToken.isPassword())
    {
        m_remotePeerCredentials.setUser(client->credentials().username.c_str());
        m_remotePeerCredentials.setPassword(client->credentials().authToken.value.c_str());
    }

    if (getState() == QnTransactionTransportBase::Error || getState() == QnTransactionTransportBase::Closed)
    {
        return;
    }

    nx::network::http::HttpHeaders::const_iterator contentTypeIter = client->response()->headers.find("Content-Type");
    if (contentTypeIter == client->response()->headers.end())
    {
        NX_WARNING(this, nx::format("Remote transaction server (%1) did not specify Content-Type in response. Aborting connection...")
            .arg(client->url()));
        cancelConnecting();
        return;
    }

    if (!m_multipartContentParser->setContentType(contentTypeIter->second))
    {
        NX_WARNING(this, "Remote transaction server (%1) specified Content-Type (%2) which does not define multipart HTTP content",
            client->url(), contentTypeIter->second);
        cancelConnecting();
        return;
    }

    //TODO #akolesnikov check Content-Type (to support different transports)

    auto contentEncodingIter = client->response()->headers.find("Content-Encoding");
    if (contentEncodingIter != client->response()->headers.end())
    {
        if (contentEncodingIter->second == "gzip")
        {
            //enabling decompression of received transactions
            auto ungzip = std::make_shared<nx::utils::bstream::gzip::Uncompressor>();
            ungzip->setNextFilter(std::move(m_incomingTransactionStreamParser));
            m_incomingTransactionStreamParser = std::move(ungzip);
        }
        else
        {
            //TODO #akolesnikov unsupported Content-Encoding ?
        }
    }

    auto isCloudIter = client->response()->headers.find("X-Nx-Cloud");
    if (isCloudIter != client->response()->headers.end())
        setState(ConnectingStage2);

    auto data = m_httpClient->fetchMessageBodyBuffer();

    if (getState() == ConnectingStage1)
    {
        if (m_connectionLockGuard->tryAcquireConnecting())
        {
            setState(ConnectingStage2);
            NX_ASSERT(data.empty());
        }
        else
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            QUrlQuery query = QUrlQuery(m_remoteAddr.toQUrl());
            query.addQueryItem("canceled", QString());
            m_remoteAddr.setQuery(query);
        }
        repeatDoGet();
    }
    else
    {
        if (nx::network::http::getHeaderValue(
            m_httpClient->response()->headers,
            Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME) == "true")
        {
            //inserting base64 decoder before the last filter
            m_incomingTransactionStreamParser = nx::utils::bstream::insert(
                m_incomingTransactionStreamParser,
                nx::utils::bstream::last(m_incomingTransactionStreamParser),
                std::make_shared<nx::network::http::Base64DecoderFilter>());

            //base64-encoded data contains multiple transactions so
            //    inserting sized data decoder after base64 decoder
            m_incomingTransactionStreamParser = nx::utils::bstream::insert(
                m_incomingTransactionStreamParser,
                nx::utils::bstream::last(m_incomingTransactionStreamParser),
                std::make_shared<nx::utils::bstream::SizedDataDecodingFilter>());
        }

        auto keepAliveHeaderIter = m_httpClient->response()->headers.find(Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME);
        // Servers 3.1 and above sends keep-alive to the client
        if (keepAliveHeaderIter != m_httpClient->response()->headers.end() &&
            m_remotePeerEcProtoVersion >= kMinServerVersionWithClientKeepAlive)
        {
            m_remotePeerSupportsKeepAlive = true;
            nx::network::http::header::KeepAlive keepAliveHeader;
            if (keepAliveHeader.parse(keepAliveHeaderIter->second))
                m_tcpKeepAliveTimeout = std::max(
                    std::chrono::duration_cast<std::chrono::seconds>(m_tcpKeepAliveTimeout),
                    keepAliveHeader.timeout);
        }

        m_incomingDataSocket = m_httpClient->takeSocket();
        if (m_connectionType == ConnectionType::bidirectional)
        {
            NX_CRITICAL(0, "Bidirection mode is not supported any more");
            //m_outgoingDataSocket = m_incomingDataSocket;
        }

        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            startSendKeepAliveTimerNonSafe();
        }

        m_httpClient.reset();
        if (m_connectionLockGuard->tryAcquireConnected())
        {
            setExtraDataBuffer(std::move(data));
            setState(QnTransactionTransportBase::Connected);
        }
        else
        {
            cancelConnecting();
        }
    }
}

void QnTransactionTransportBase::at_httpClientDone(const nx::network::http::AsyncHttpClientPtr& client)
{
    NX_VERBOSE(this, "at_httpClientDone");

    if (client->failed())
    {
        NX_DEBUG(this, nx::format("Http request failed %1").arg(client->lastSysErrorCode()));
        cancelConnecting();
    }
}

void QnTransactionTransportBase::processTransactionData(const nx::Buffer& data)
{
    NX_ASSERT(m_peerRole == prOriginating);
    if (!m_incomingTransactionStreamParser->processData(data))
    {
        NX_WARNING(this,
            "Error processing incoming data (4) from peer %1. Disconnecting...",
            m_remotePeer.id.toString());
        return setStateNoLock(State::Error);
    }
}

bool QnTransactionTransportBase::isReadyToSend(ApiCommand::Value command) const
{
    if (m_state == ReadyForStreaming)
    {
        // allow to send system command immediately, without tranSyncRequest
        return (command != ApiCommand::NotDefined && ApiCommand::isSystem(command)) ? true : m_writeSync;
    }
    else
    {
        return false;
    }
}

bool QnTransactionTransportBase::isReadSync(ApiCommand::Value command) const
{
    if (m_state == ReadyForStreaming)
    {
        // allow to read system command immediately, without tranSyncRequest
        return (command != ApiCommand::NotDefined && ApiCommand::isSystem(command)) ? true : m_readSync;
    }
    else
    {
        return false;
    }
}

const char* QnTransactionTransportBase::toString(State state)
{
    switch (state)
    {
        case NotDefined:
            return "NotDefined";
        case ConnectingStage1:
            return "ConnectingStage1";
        case ConnectingStage2:
            return "ConnectingStage2";
        case Connected:
            return "Connected";
        case NeedStartStreaming:
            return "NeedStartStreaming";
        case ReadyForStreaming:
            return "ReadyForStreaming";
        case Closed:
            return "Closed";
        case Error:
            return "Error";
        default:
            return "unknown";
    }
}

void QnTransactionTransportBase::addHttpChunkExtensions(nx::network::http::HttpHeaders* const headers)
{
    for (auto val : m_beforeSendingChunkHandlers)
        val.second(this, headers);
}

void QnTransactionTransportBase::processChunkExtensions(const nx::network::http::HttpHeaders& headers)
{
    if (headers.empty())
        return;

    for (auto val : m_httpChunkExtensonHandlers)
        val.second(this, headers);
}

void QnTransactionTransportBase::setExtraDataBuffer(nx::Buffer data)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    NX_ASSERT(m_extraData.empty());
    m_extraData = std::move(data);
}

void QnTransactionTransportBase::setRemoteIdentityTime(qint64 time)
{
    m_remoteIdentityTime = time;
}

qint64 QnTransactionTransportBase::remoteIdentityTime() const
{
    return m_remoteIdentityTime;
}

void QnTransactionTransportBase::scheduleAsyncRead()
{
    if (!m_incomingDataSocket)
        return;

    NX_ASSERT(isInSelfAioThread());
    NX_ASSERT(!m_asyncReadScheduled);

    using namespace std::placeholders;
    m_incomingDataSocket->readSomeAsync(
        &m_readBuffer,
        std::bind(&QnTransactionTransportBase::onSomeBytesRead, this, _1, _2));
    m_asyncReadScheduled = true;
    m_lastReceiveTimer.restart();
}

void QnTransactionTransportBase::startListeningNonSafe()
{
    NX_ASSERT(m_incomingDataSocket || m_outgoingDataSocket);
    m_httpStreamReader.resetState();

    post(
        [this]()
    {
        using namespace std::chrono;
        using namespace std::placeholders;

        if (!m_incomingDataSocket)
            return;

        m_incomingDataSocket->setRecvTimeout(SOCKET_TIMEOUT);
        m_incomingDataSocket->setSendTimeout(
            duration_cast<milliseconds>(kSocketSendTimeout).count());
        m_incomingDataSocket->setNonBlockingMode(true);
        m_readBuffer.reserve(m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE);

        scheduleAsyncRead();
    });
}

void QnTransactionTransportBase::postTransactionDone(const nx::network::http::AsyncHttpClientPtr& client)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    NX_ASSERT(client == m_outgoingTranClient);

    if (client->failed() || !client->response())
    {
        NX_WARNING(this,
            "Network error posting transaction to %1. system result code: %2",
            m_postTranBaseUrl.toString(), SystemError::toString(client->lastSysErrorCode()));
        setStateNoLock(Error);
        return;
    }

    DataToSend& dataCtx = m_dataToSend.front();

    if (client->response()->statusLine.statusCode == nx::network::http::StatusCode::unauthorized &&
        m_authOutgoingConnectionByServerKey)
    {
        NX_VERBOSE(this,
            "Failed to authenticate on peer %1 by key. Retrying using admin credentials...",
            m_postTranBaseUrl.toString());
        m_authOutgoingConnectionByServerKey = false;
        fillAuthInfo(m_outgoingTranClient, m_authOutgoingConnectionByServerKey);
        m_outgoingTranClient->doPost(
            m_postTranBaseUrl,
            m_base64EncodeOutgoingTransactions
            ? "application/text"
            : Qn::serializationFormatToHttpContentType(m_remotePeer.dataFormat),
            dataCtx.encodedSourceData);
        return;
    }

    if (client->response()->statusLine.statusCode != nx::network::http::StatusCode::ok)
    {
        NX_WARNING(this,
            "Server %1 returned %2 (%3) response while posting transaction",
            m_postTranBaseUrl.toString(), client->response()->statusLine.statusCode,
            client->response()->statusLine.reasonPhrase);
        setStateNoLock(Error);
        m_outgoingTranClient.reset();
        return;
    }

    m_dataToSend.pop_front();
    if (m_dataToSend.empty())
        return;

    serializeAndSendNextDataBuffer();
}

}
