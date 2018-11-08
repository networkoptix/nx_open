#include "generic_transport.h"

#include <nx/utils/log/log.h>

#include "../command_descriptor.h"
#include "../transaction_transport.h"

namespace nx::data_sync_engine::transport {

/** Holding in queue not more then this transaction count. */
static constexpr int kMaxTransactionsPerIteration = 17;

GenericTransport::GenericTransport(
    const ProtocolVersionRange& protocolVersionRange,
    TransactionLog* const transactionLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const std::string& systemId,
    const ConnectionRequestAttributes& connectionRequestAttributes,
    const vms::api::PeerData& localPeer,
    std::unique_ptr<AbstractCommandPipeline> commandPipeline)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_systemId(systemId),
    m_localPeer(localPeer),
    m_remotePeer(connectionRequestAttributes.remotePeer),
    m_commandPipeline(std::move(commandPipeline)),
    m_transactionLogReader(std::make_unique<TransactionLogReader>(
        transactionLog,
        systemId.c_str(),
        connectionRequestAttributes.remotePeer.dataFormat,
        outgoingCommandFilter)),
    m_commonTransportHeaderOfRemoteTransaction(protocolVersionRange.currentVersion())
{
    bindToAioThread(m_commandPipeline->getAioThread());

    m_commonTransportHeaderOfRemoteTransaction.connectionId =
        connectionRequestAttributes.connectionId;
    m_commonTransportHeaderOfRemoteTransaction.systemId = systemId;
    m_commonTransportHeaderOfRemoteTransaction.peerId =
        connectionRequestAttributes.remotePeer.id.toSimpleByteArray().toStdString();
    m_commonTransportHeaderOfRemoteTransaction.endpoint =
        m_commandPipeline->remotePeerEndpoint();
    m_commonTransportHeaderOfRemoteTransaction.vmsTransportHeader.sender =
        connectionRequestAttributes.remotePeer.id;
    m_commonTransportHeaderOfRemoteTransaction.transactionFormatVersion =
        connectionRequestAttributes.remotePeerProtocolVersion;

    m_commandPipeline->setOnConnectionClosed(
        [this](auto... args) { processConnectionClosedEvent(std::move(args)...); });

    m_commandPipeline->setOnGotTransaction(
        [this](auto&&... args) { processCommandData(std::move(args)...); });
}

GenericTransport::~GenericTransport()
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("systemId %1. Closing connection to %2")
        .args(m_systemId, m_commonTransportHeaderOfRemoteTransaction));
}

void GenericTransport::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_commandPipeline)
        m_commandPipeline->bindToAioThread(aioThread);
    if (m_transactionLogReader)
        m_transactionLogReader->bindToAioThread(aioThread);
}

network::SocketAddress GenericTransport::remotePeerEndpoint() const
{
    return m_commandPipeline->remotePeerEndpoint();
}

ConnectionClosedSubscription& GenericTransport::connectionClosedSubscription()
{
    return m_connectionClosedSubscription;
}

void GenericTransport::setOnGotTransaction(
    CommandHandler handler)
{
    m_gotCommandHandler = std::move(handler);
}

QnUuid GenericTransport::connectionGuid() const
{
    return m_commandPipeline->connectionGuid();
}

const TransactionTransportHeader& 
    GenericTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransportHeaderOfRemoteTransaction;
}

void GenericTransport::sendTransaction(
    TransactionTransportHeader transportHeader,
    const std::shared_ptr<const SerializableAbstractTransaction>& transactionSerializer)
{
    transportHeader.vmsTransportHeader.fillSequence(
        m_localPeer.id,
        m_localPeer.instanceId);

    post(
        [this, transportHeader = std::move(transportHeader), transactionSerializer]()
        {
            if (::ec2::ApiCommand::isSystem(transactionSerializer->header().command)
                || m_canSendCommands)
            {
                m_commandPipeline->sendTransaction(
                    std::move(transportHeader),
                    transactionSerializer);
                return;
            }

            NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this), lm("Postponing send transaction %1 to %2")
                .args(transactionSerializer->header().command,
                    m_commonTransportHeaderOfRemoteTransaction));

            //cannot send transaction right now: updating local transaction sequence
            const vms::api::PersistentIdData tranStateKey(
                transactionSerializer->header().peerID,
                transactionSerializer->header().persistentInfo.dbID);
            m_tranStateToSynchronizeTo.values[tranStateKey] =
                transactionSerializer->header().persistentInfo.sequence;
            //transaction will be sent later
        });
}

void GenericTransport::start()
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
        lm("Starting outgoing transaction channel to %1")
            .arg(m_commonTransportHeaderOfRemoteTransaction));

    // Sending tranSyncRequest.
    auto requestTran = command::make<command::TranSyncRequest>(
        m_localPeer.id);
    requestTran.params.persistentState = m_transactionLogReader->getCurrentState();

    TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.vmsTransportHeader.processedPeers << m_remotePeer.id;
    transportHeader.vmsTransportHeader.processedPeers << m_localPeer.id;

    sendTransaction(
        std::move(transportHeader),
        makeSerializer<command::TranSyncRequest>(std::move(requestTran)));
}

void GenericTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    Command<vms::api::SyncRequestData> data,
    TransactionProcessedHandler handler)
{
    m_tranStateToSynchronizeTo = m_transactionLogReader->getCurrentState();
    m_remotePeerTranState = std::move(data.params.persistentState);

    //sending sync response
    auto tranSyncResponse = command::make<command::TranSyncResponse>(
        m_localPeer.id);
    tranSyncResponse.params.result = 0;

    TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.vmsTransportHeader.processedPeers.insert(
        m_localPeer.id);

    sendTransaction(
        std::move(transportHeader),
        makeSerializer<command::TranSyncResponse>(std::move(tranSyncResponse)));

    m_haveToSendSyncDone = true;

    // Starting transactions delivery.

    ReadCommandsFilter filter;
    filter.from = m_remotePeerTranState;
    filter.to = m_tranStateToSynchronizeTo;
    filter.maxTransactionsToReturn = kMaxTransactionsPerIteration;

    m_transactionLogReader->readTransactions(
        filter,
        [this](auto... args) { onTransactionsReadFromLog(std::move(args)...); });

    handler(ResultCode::ok);
}

void GenericTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    Command<vms::api::TranStateResponse> /*data*/,
    TransactionProcessedHandler /*handler*/)
{
    // TODO: no need to do anything?
    //NX_ASSERT(false);
}

void GenericTransport::processSpecialTransaction(
    const TransactionTransportHeader& /*transportHeader*/,
    Command<vms::api::TranSyncDoneData> /*data*/,
    TransactionProcessedHandler /*handler*/)
{
    // TODO: no need to do anything?
    //NX_ASSERT(false);
}

AbstractCommandPipeline& GenericTransport::commandPipeline()
{
    return *m_commandPipeline;
}

void GenericTransport::stopWhileInAioThread()
{
    m_commandPipeline.reset();
    m_transactionLogReader.reset();
}

void GenericTransport::processConnectionClosedEvent(
    SystemError::ErrorCode closeReason)
{
    m_connectionClosedSubscription.notify(closeReason);
}

void GenericTransport::processCommandData(
    Qn::SerializationFormat dataFormat,
    const QByteArray& serializedCommand,
    TransactionTransportHeader transportHeader)
{
    // TODO: #ak Processing specific commands.

    if (!m_gotCommandHandler)
        return;
    
    auto commandData = TransactionDeserializer::deserialize(
        dataFormat,
        transportHeader.peerId,
        transportHeader.transactionFormatVersion,
        std::move(serializedCommand));

    if (!commandData)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("Failed to deserialized %1 command received from (%2, %3)")
            .args(dataFormat, transportHeader.systemId, transportHeader.endpoint.toString()));
        m_commandPipeline->closeConnection();
        return;
    }

    m_gotCommandHandler(
        std::move(commandData),
        std::move(transportHeader));
}

void GenericTransport::onTransactionsReadFromLog(
    ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    vms::api::TranState readedUpTo)
{
    // TODO: handle api::ResultCode::tryLater result code

    if ((resultCode != ResultCode::ok) && (resultCode != ResultCode::partialContent))
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
            lm("systemId %1. Error reading transaction log (%2). Closing connection to the peer %3")
                .args(m_systemId, toString(resultCode),
                    m_commonTransportHeaderOfRemoteTransaction));
        m_commandPipeline->closeConnection();
        return;
    }

    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
        lm("systemId %1. Read %2 transactions from transaction log (result %3). "
           "Posting them to the send queue to %4")
            .args(m_systemId, serializedTransactions.size(), toString(resultCode),
                m_commonTransportHeaderOfRemoteTransaction));

    // Posting transactions to send
    for (auto& tranData: serializedTransactions)
    {
        TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
        transportHeader.systemId = m_systemId;
        transportHeader.vmsTransportHeader.distance = 1;
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_localPeer.id);

        m_commandPipeline->sendTransaction(
            transportHeader,
            std::move(tranData.serializer));
    }

    m_remotePeerTranState = readedUpTo;

    if (resultCode == ResultCode::partialContent
        || m_tranStateToSynchronizeTo > m_remotePeerTranState)
    {
        if (resultCode != ResultCode::partialContent)
        {
            // TODO: Printing remote and local states.
        }

        //< Local state could be updated while we were synchronizing remote peer
        // Continuing reading transactions.
        m_transactionLogReader->readTransactions(
            ReadCommandsFilter{
                m_remotePeerTranState,
                m_tranStateToSynchronizeTo,
                kMaxTransactionsPerIteration,
                {}},
            [this](auto... args) { onTransactionsReadFromLog(std::move(args)...); });
        return;
    }

    // Sending transactions to remote peer is allowed now
    enableOutputChannel();
}

void GenericTransport::enableOutputChannel()
{
    NX_DEBUG(QnLog::EC2_TRAN_LOG.join(this),
        lm("systemId %1. Enabled output channel to the peer %2")
            .args(m_systemId, m_commonTransportHeaderOfRemoteTransaction));

    m_canSendCommands = true;

    if (m_haveToSendSyncDone)
    {
        m_haveToSendSyncDone = false;

        auto tranSyncDone = command::make<command::TranSyncDone>(
            m_localPeer.id);
        tranSyncDone.params.result = 0;

        TransactionTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_localPeer.id);

        sendTransaction(
            std::move(transportHeader),
            makeSerializer<command::TranSyncDone>(std::move(tranSyncDone)));
    }
}

} // namespace nx::data_sync_engine::transport
