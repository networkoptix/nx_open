#pragma once

#include <nx_ec/data/api_discovery_data.h>
#include <nx_ec/ec_api.h>
#include <nx/vms/discovery/manager.h>
#include <transaction/transaction.h>

namespace ec2
{


template<class QueryProcessorType>
class QnDiscoveryManager: public AbstractDiscoveryManager
{
public:
    QnDiscoveryManager(
        QueryProcessorType* const queryProcessor,
        const Qn::UserAccessData &userAccessData);
    virtual ~QnDiscoveryManager();

protected:
    virtual int discoverPeer(const QnUuid &id, const nx::utils::Url &url, impl::SimpleHandlerPtr handler) override;
    virtual int addDiscoveryInformation(const QnUuid &id, const nx::utils::Url &url, bool ignore, impl::SimpleHandlerPtr handler) override;
    virtual int removeDiscoveryInformation(const QnUuid &id, const nx::utils::Url &url, bool ignore, impl::SimpleHandlerPtr handler) override;
    virtual int getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler) override;
private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;
};
ApiDiscoveryData toApiDiscoveryData(const QnUuid &id, const nx::utils::Url &url, bool ignore);

// TODO: Could probably be moved to mediaserver, as it is used only there.
class QnDiscoveryMonitor: public QObject, public QnCommonModuleAware
{
public:
    QnDiscoveryMonitor(TransactionMessageBusAdapter* messageBus);
    virtual ~QnDiscoveryMonitor();

private:
    void clientFound(QnUuid peerId, Qn::PeerType peerType);
    void serverFound(nx::vms::discovery::ModuleEndpoint module);
    void serverLost(QnUuid id);

    template<typename Transaction, typename Target>
    void send(ApiCommand::Value command, Transaction data, const Target& target);

private:
    TransactionMessageBusAdapter* m_messageBus;
    std::map<QnUuid, ApiDiscoveredServerData> m_serverCache;
};

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::QnDiscoveryManager(
    QueryProcessorType * const queryProcessor,
    const Qn::UserAccessData &userAccessData)
:
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::~QnDiscoveryManager() {}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(
    const QnUuid &id,
    const nx::utils::Url& url,
    impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    ApiDiscoverPeerData params;
    params.id = id;
    params.url = url.toString();

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::discoverPeer,
        params,
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::addDiscoveryInformation(
        const QnUuid &id,
        const nx::utils::Url &url,
        bool ignore,
        impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::addDiscoveryInformation,
        toApiDiscoveryData(id, url, ignore),
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::removeDiscoveryInformation(
    const QnUuid &id,
    const nx::utils::Url &url,
    bool ignore,
    impl::SimpleHandlerPtr handler)
{
    const int reqId = generateRequestID();
    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeDiscoveryInformation,
        toApiDiscoveryData(id, url, ignore),
        [handler, reqId](ErrorCode errorCode)
        {
            handler->done(reqId, errorCode);
        }
    );

    return reqId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::getDiscoveryData(impl::GetDiscoveryDataHandlerPtr handler)
{
    const int reqID = generateRequestID();

    auto queryDoneHandler = [reqID, handler](ErrorCode errorCode, const ApiDiscoveryDataList &data)
    {
        ApiDiscoveryDataList outData;
        if (errorCode == ErrorCode::ok)
            outData = data;
        handler->done(reqID, errorCode, outData);
    };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<
            QnUuid, ApiDiscoveryDataList, decltype(queryDoneHandler)>(
                ApiCommand::getDiscoveryData, QnUuid(), queryDoneHandler);
    return reqID;
}

} // namespace ec2
