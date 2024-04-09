// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx_ec/managers/abstract_media_server_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnMediaServerManager: public AbstractMediaServerManager
{
public:
    QnMediaServerManager(
        QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord);

    virtual int getServers(
        Handler<nx::vms::api::MediaServerDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getServersEx(
        Handler<nx::vms::api::MediaServerDataExList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::MediaServerData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveUserAttributes(
        const nx::vms::api::MediaServerUserAttributesDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int saveStorages(
        const nx::vms::api::StorageDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeStorages(
        const nx::vms::api::IdDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getUserAttributes(
        const nx::Uuid& mediaServerId,
        Handler<nx::vms::api::MediaServerUserAttributesDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getStorages(
        const nx::Uuid& mediaServerId,
        Handler<nx::vms::api::StorageDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_auditRecord); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::audit::Record m_auditRecord;
};

template<class QueryProcessorType>
QnMediaServerManager<QueryProcessorType>::QnMediaServerManager(
    QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord)
    :
    m_queryProcessor(queryProcessor),
    m_auditRecord(auditRecord)
{}

template<class T>
int QnMediaServerManager<T>::getServers(
    Handler<nx::vms::api::MediaServerDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::MediaServerDataList> (
        ApiCommand::getMediaServers,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::getServersEx(
    Handler<nx::vms::api::MediaServerDataExList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::MediaServerDataExList>(
        ApiCommand::getMediaServersEx,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::save(
    const nx::vms::api::MediaServerData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveMediaServer,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::remove(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeMediaServer,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::saveUserAttributes(
    const nx::vms::api::MediaServerUserAttributesDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveMediaServerUserAttributesList,
        dataList,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::saveStorages(
    const nx::vms::api::StorageDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveStorages,
        dataList,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::removeStorages(
    const nx::vms::api::IdDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeStorages,
        dataList,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::getUserAttributes(
    const nx::Uuid& mediaServerId,
    Handler<nx::vms::api::MediaServerUserAttributesDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    using namespace nx::vms::api;
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, MediaServerUserAttributesDataList>(
        ApiCommand::getMediaServerUserAttributesList,
        mediaServerId,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnMediaServerManager<T>::getStorages(
    const nx::Uuid& mediaServerId,
    Handler<nx::vms::api::StorageDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    using namespace nx::vms::api;
    const int requestId = generateRequestID();
    processor().template processQueryAsync<StorageParentId, StorageDataList>(
        ApiCommand::getStorages,
        mediaServerId,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
