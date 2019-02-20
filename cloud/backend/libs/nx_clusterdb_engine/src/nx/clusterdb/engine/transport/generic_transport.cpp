#include "generic_transport.h"

#include <nx/utils/log/log.h>

#include "../command_descriptor.h"
#include "../outgoing_command_filter.h"
#include "../transaction_transport.h"

namespace nx::clusterdb::engine::transport {

/** Holding in queue not more then this transaction count. */
static constexpr int kMaxTransactionsPerIteration = 17;

GenericTransport::GenericTransport(
    const ProtocolVersionRange& protocolVersionRange,
    CommandLog* const transactionLog,
    const OutgoingCommandFilter& outgoingCommandFilter,
    const std::string& systemId,
    const ConnectionRequestAttributes& connectionRequestAttributes,
    const vms::api::PeerData& localPeer,
    std::unique_ptr<AbstractCommandPipeline> commandPipeline)
    :
    m_protocolVersionRange(protocolVersionRange),
    m_outgoingCommandFilter(outgoingCommandFilter),
    m_systemId(systemId),
    m_localPeer(localPeer),
    m_remotePeer(connectionRequestAttributes.remotePeer),
    m_commandPipeline(std::move(commandPipeline)),
    m_transactionLogReader(std::make_unique<CommandLogReader>(
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
    NX_DEBUG(this, lm("systemId %1. Closing connection to %2")
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

std::string GenericTransport::connectionGuid() const
{
    return m_commandPipeline->connectionGuid();
}

const CommandTransportHeader&
    GenericTransport::commonTransportHeaderOfRemoteTransaction() const
{
    return m_commonTransportHeaderOfRemoteTransaction;
}

void GenericTransport::sendTransaction(
    CommandTransportHeader transportHeader,
    const std::shared_ptr<const SerializableAbstractCommand>& transactionSerializer)
{
    post(
        [this, transportHeader = std::move(transportHeader), transactionSerializer]()
        {
            if (peerAlreadyHasCommand(transactionSerializer->header()))
            {
                NX_VERBOSE(this,
                    "Not sending command %1 to %2 since remote peer must have it",
                    engine::toString(transactionSerializer->header()),
                    m_commonTransportHeaderOfRemoteTransaction);
                return;
            }

            if (isHandshakeCommand(transactionSerializer->header().command)
                || m_canSendCommands)
            {
                NX_VERBOSE(this, "Sending command %1 to %2",
                    engine::toString(transactionSerializer->header()),
                    m_commonTransportHeaderOfRemoteTransaction);
                m_commandPipeline->sendTransaction(
                    std::move(transportHeader),
                    transactionSerializer);
                return;
            }

            NX_DEBUG(this, lm("Postponing sending command %1 to %2")
                .args(engine::toString(transactionSerializer->header()),
                    m_commonTransportHeaderOfRemoteTransaction));

            //cannot send transaction right now: updating local transaction sequence
            const vms::api::PersistentIdData tranStateKey(
                transactionSerializer->header().peerID,
                transactionSerializer->header().persistentInfo.dbID);
            m_tranStateToSynchronizeTo.values[tranStateKey] = std::max(
                m_tranStateToSynchronizeTo.values[tranStateKey],
                transactionSerializer->header().persistentInfo.sequence);
            //transaction will be sent later
        });
}

void GenericTransport::start()
{
    NX_DEBUG(this,
        lm("Starting outgoing transaction channel to %1")
            .arg(m_commonTransportHeaderOfRemoteTransaction));

    m_commandPipeline->start();

    // Sending tranSyncRequest.
    auto requestTran = command::make<command::TranSyncRequest>(
        m_localPeer.id);
    requestTran.params.persistentState = m_transactionLogReader->getCurrentState();

    CommandTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.vmsTransportHeader.processedPeers << m_remotePeer.id;
    transportHeader.vmsTransportHeader.processedPeers << m_localPeer.id;

    sendTransaction(
        std::move(transportHeader),
        makeSerializer<command::TranSyncRequest>(std::move(requestTran)));
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
    CommandTransportHeader transportHeader)
{
    auto commandData = TransactionDeserializer::deserialize(
        dataFormat,
        transportHeader.peerId,
        transportHeader.transactionFormatVersion,
        std::move(serializedCommand));

    if (!commandData)
    {
        NX_DEBUG(this,
            lm("Failed to deserialized %1 command received from (%2, %3)")
            .args(dataFormat, transportHeader.systemId, transportHeader.endpoint.toString()));
        m_commandPipeline->closeConnection();
        return;
    }

    if (m_remotePeer.persistentId.isNull() &&
        !commandData->header().persistentInfo.dbID.isNull())
    {
        m_remotePeer.persistentId = commandData->header().persistentInfo.dbID;
    }

    if (isHandshakeCommand(commandData->header().command))
    {
        processHandshakeCommand(
            std::move(transportHeader),
            std::move(commandData));
        return;
    }

    if (!m_gotCommandHandler)
        return;

    m_gotCommandHandler(
        std::move(commandData),
        std::move(transportHeader));
}

bool GenericTransport::peerAlreadyHasCommand(const CommandHeader& header) const
{
    return header.peerID == m_remotePeer.id
        && header.persistentInfo.dbID == m_remotePeer.persistentId;
}

bool GenericTransport::isHandshakeCommand(int commandType) const
{
    return commandType == command::TranSyncRequest::code
        || commandType == command::TranSyncResponse::code
        || commandType == command::TranSyncDone::code;
}

void GenericTransport::processHandshakeCommand(
    CommandTransportHeader transportHeader,
    std::unique_ptr<DeserializableCommandData> commandData)
{
    if (commandData->header().command == command::TranSyncRequest::code)
    {
        auto commandWrapper = commandData->deserialize<command::TranSyncRequest>(
            transportHeader.transactionFormatVersion);
        processHandshakeCommand(commandWrapper->take());
    }
}

void GenericTransport::processHandshakeCommand(
    Command<vms::api::SyncRequestData> data)
{
    m_tranStateToSynchronizeTo =
        m_outgoingCommandFilter.filter(m_transactionLogReader->getCurrentState());
    m_remotePeerTranState = std::move(data.params.persistentState);

    //sending sync response
    auto tranSyncResponse = command::make<command::TranSyncResponse>(
        m_localPeer.id);
    tranSyncResponse.params.result = 0;

    CommandTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.vmsTransportHeader.processedPeers.insert(
        m_localPeer.id);

    sendTransaction(
        std::move(transportHeader),
        makeSerializer<command::TranSyncResponse>(std::move(tranSyncResponse)));

    m_haveToSendSyncDone = true;

    // TODO: #ak If m_remotePeerTranState contains everything that m_tranStateToSynchronizeTo does,
    // then not reading log and starting sending commands.

    // TODO: #ak It is possible that m_remotePeerTranState contains sequences higher than
    // m_tranStateToSynchronizeTo.
    ReadCommandsFilter filter;
    filter.from = m_remotePeerTranState;
    filter.to = m_tranStateToSynchronizeTo;
    filter.maxTransactionsToReturn = kMaxTransactionsPerIteration;

    m_transactionLogReader->readTransactions(
        filter,
        [this](auto&&... args) { onTransactionsReadFromLog(std::move(args)...); });
}

void GenericTransport::onTransactionsReadFromLog(
    ResultCode resultCode,
    std::vector<dao::TransactionLogRecord> serializedTransactions,
    vms::api::TranState readedUpTo)
{
    // TODO: handle api::ResultCode::tryLater result code

    if ((resultCode != ResultCode::ok) && (resultCode != ResultCode::partialContent))
    {
        NX_DEBUG(this,
            lm("systemId %1. Error reading transaction log (%2). Closing connection to the peer %3")
                .args(m_systemId, toString(resultCode),
                    m_commonTransportHeaderOfRemoteTransaction));
        m_commandPipeline->closeConnection();
        return;
    }

    NX_DEBUG(this,
        lm("systemId %1. Read %2 transactions from transaction log (result %3). "
           "Posting them to the send queue to %4")
            .args(m_systemId, serializedTransactions.size(), toString(resultCode),
                m_commonTransportHeaderOfRemoteTransaction));

    sendTransactions(std::exchange(serializedTransactions, {}));

    // TODO: #ak If m_remotePeerTranState contained unknown peers, than they are now
    // missing in readedUpTo.
    // So, here we need to unite sets m_remotePeerTranState and readedUpTo.
    m_remotePeerTranState = readedUpTo;

    if (resultCode == ResultCode::partialContent ||
        m_tranStateToSynchronizeTo.containsDataMissingIn(m_remotePeerTranState))
    {
        NX_DEBUG(this,
            lm("systemId %1. Synchronize to (%2), already synchronized to (%3)")
            .args(m_systemId, stateToString(m_tranStateToSynchronizeTo),
                stateToString(m_remotePeerTranState)));

        // Asserting that something new has been read.
        NX_ASSERT(!m_prevReadResult || readedUpTo.containsDataMissingIn(*m_prevReadResult));
        m_prevReadResult = readedUpTo;

        // Local state could have been updated while we were synchronizing remote peer
        // Continuing reading transactions.
        m_transactionLogReader->readTransactions(
            ReadCommandsFilter{
                m_remotePeerTranState,
                m_tranStateToSynchronizeTo,
                kMaxTransactionsPerIteration,
                {}},
            [this](auto&&... args) { onTransactionsReadFromLog(std::move(args)...); });
        return;
    }
    else
    {
        NX_DEBUG(this, lm("systemId %1. "
            "Done initial synchronization to (%2)")
                .args(m_systemId, stateToString(m_remotePeerTranState)));
    }

    // Sending transactions to remote peer is allowed now.
    enableOutputChannel();
}

void GenericTransport::sendTransactions(
    std::vector<dao::TransactionLogRecord> serializedTransactions)
{
    for (auto& tranData: serializedTransactions)
    {
        CommandTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
        transportHeader.systemId = m_systemId;
        transportHeader.vmsTransportHeader.distance = 1;
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_localPeer.id);

        m_commandPipeline->sendTransaction(
            transportHeader,
            std::move(tranData.serializer));
    }
}

void GenericTransport::enableOutputChannel()
{
    NX_DEBUG(this,
        lm("systemId %1. Enabled output channel to the peer %2")
            .args(m_systemId, m_commonTransportHeaderOfRemoteTransaction));

    m_canSendCommands = true;

    if (m_haveToSendSyncDone)
    {
        m_haveToSendSyncDone = false;

        auto tranSyncDone = command::make<command::TranSyncDone>(
            m_localPeer.id);
        tranSyncDone.params.result = 0;

        CommandTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
        transportHeader.vmsTransportHeader.processedPeers.insert(
            m_localPeer.id);

        sendTransaction(
            std::move(transportHeader),
            makeSerializer<command::TranSyncDone>(std::move(tranSyncDone)));
    }
}

} // namespace nx::clusterdb::engine::transport
