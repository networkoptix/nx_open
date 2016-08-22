/**********************************************************
* Aug 8, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <list>
#include <map>
#include <memory>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include <cdb/result_code.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx_ec/data/api_peer_data.h>
#include <nx_ec/data/api_tran_state_data.h>

#include <utils/common/counter.h>

#include "access_control/auth_types.h"
#include "transaction_processor.h"
#include "transaction_transport_header.h"


namespace nx_http {
class HttpServerConnection;
class MessageDispatcher;
}   // namespace nx_http

namespace nx {
namespace cdb {

class AuthorizationManager;
namespace conf {
class Settings;
}   //namespace conf

namespace ec2 {

class IncomingTransactionDispatcher;
class TransactionLog;
class TransactionTransport;
class TransactionTransportHeader;

/** Managers ec2 transaction connections from mediaservers */
class ConnectionManager
{
public:
    ConnectionManager(
        const conf::Settings& settings,
        TransactionLog* const transactionLog,
        IncomingTransactionDispatcher* const transactionDispatcher);
    virtual ~ConnectionManager();

    /** mediaserver calls this method to open 2-way transaction exchange channel */
    void createTransactionConnection(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::HttpRequestProcessedHandler completionHandler);
    /** mediaserver uses this method to push transactions */
    void pushTransaction(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::HttpRequestProcessedHandler completionHandler);

    /** dispatches transaction to the right peers */
    template<class T>
    void dispatchTransaction(
        const nx::String& systemId,
        ::ec2::QnTransaction<T> transaction,
        TransactionTransportHeader transportHeader)
    {
        QnMutexLocker lk(&m_mutex);
        const auto& connectionBySystemIdAndPeerIdIndex =
            m_connections.get<kConnectionBySystemIdAndPeerIdIndex>();

        std::size_t connectionCount = 0;
        std::array<TransactionTransport*, 7> connectionsToSendTo;
        auto connectionIt = connectionBySystemIdAndPeerIdIndex
            .lower_bound(std::make_pair(systemId, nx::String()));
        for ( ;
              connectionIt != connectionBySystemIdAndPeerIdIndex.end()
                  && connectionIt->systemIdAndPeerId.first == systemId;
             ++connectionIt)
        {
            if (connectionIt->systemIdAndPeerId.second == transaction.peerID)
                continue;

            connectionsToSendTo[connectionCount++] = connectionIt->connection;
            if (connectionCount < connectionsToSendTo.size())
                continue;

            for (auto& connection: connectionsToSendTo)
                connection->sendTransaction(transaction, transportHeader);
            connectionCount = 0;
        }

        if (connectionCount == 1)
        {
            //minor optimization
            connectionsToSendTo[0]->sendTransaction(
                std::move(transaction),
                std::move(transportHeader));
        }
        else
        {
            for (auto& connection : connectionsToSendTo)
                connection->sendTransaction(transaction, transportHeader);
        }
    }

private:
    struct ConnectionContext
    {
        std::unique_ptr<TransactionTransport> connection;
        nx::String connectionId;
        //TODO #ak get rid of pair and find out how build multipart inde correctly
        std::pair<nx::String, nx::String> systemIdAndPeerId;
    };

    typedef boost::multi_index::multi_index_container<
        ConnectionContext,
        boost::multi_index::indexed_by<
            //indexing by connectionId
            boost::multi_index::ordered_unique<boost::multi_index::member<
                ConnectionContext,
                nx::String,
                &ConnectionContext::connectionId>>,
            //indexing by (systemId, peerId)
            boost::multi_index::ordered_unique<boost::multi_index::member<
                ConnectionContext,
                std::pair<nx::String, nx::String>,
                &ConnectionContext::systemIdAndPeerId>>
        >
    > ConnectionDict;

    constexpr static const int kConnectionByIdIndex = 0;
    constexpr static const int kConnectionBySystemIdAndPeerIdIndex = 1;

    const conf::Settings& m_settings;
    TransactionLog* const m_transactionLog;
    IncomingTransactionDispatcher* const m_transactionDispatcher;
    const ::ec2::ApiPeerData m_localPeerData;
    ConnectionDict m_connections;
    std::map<TransactionTransport*, std::unique_ptr<TransactionTransport>> m_connectionsToRemove;
    QnMutex m_mutex;
    QnCounter m_startedAsyncCallsCounter;

    void addNewConnection(ConnectionContext connectionContext);
    template<int connectionIndexNumber, typename ConnectionKeyType>
        void removeExistingConnection(
            QnMutexLockerBase* const /*lock*/,
            ConnectionKeyType connectionKey);
    void removeConnection(const nx::String& connectionId);
    void onGotTransaction(
        const nx::String& connectionId,
        Qn::SerializationFormat tranFormat,
        const QByteArray& data,
        TransactionTransportHeader transportHeader);
    void onTransactionDone(const nx::String& connectionId, api::ResultCode resultCode);
    void fetchDataFromConnectRequest(
        const nx_http::Request& request,
        ::ec2::ApiPeerData* const remotePeer,
        nx::String* const contentEncoding);
    void processSyncRequest(
        const nx::String& systemId,
        const TransactionTransportHeader& transportHeader,
        ::ec2::ApiSyncRequestData data,
        TransactionProcessedHandler handler);
    void processSyncResponse(
        const nx::String& systemId,
        const TransactionTransportHeader& transportHeader,
        ::ec2::QnTranStateResponse data,
        TransactionProcessedHandler handler);
    void processSyncDone(
        const nx::String& systemId,
        const TransactionTransportHeader& transportHeader,
        ::ec2::ApiTranSyncDoneData data,
        TransactionProcessedHandler handler);
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
