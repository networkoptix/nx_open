// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/rest/user_access_data.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnVideowallManager: public AbstractVideowallManager
{
public:
    QnVideowallManager(QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord);

    virtual int getVideowalls(
        Handler<nx::vms::api::VideowallDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::VideowallData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const nx::Uuid& id,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int sendControlMessage(
        const nx::vms::api::VideowallControlMessageData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_auditRecord); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::audit::Record m_auditRecord;
};

template<class QueryProcessorType>
QnVideowallManager<QueryProcessorType>::QnVideowallManager(
    QueryProcessorType* queryProcessor,
    const nx::network::rest::audit::Record& auditRecord)
    :
    m_queryProcessor(queryProcessor),
    m_auditRecord(auditRecord)
{
}

template<class QueryProcessorType>
int QnVideowallManager<QueryProcessorType>::getVideowalls(
    Handler<nx::vms::api::VideowallDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::VideowallDataList>(
        ApiCommand::getVideowalls,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnVideowallManager<T>::save(
    const nx::vms::api::VideowallData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::saveVideowall,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnVideowallManager<T>::remove(
    const nx::Uuid& id,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeVideowall,
        nx::vms::api::IdData(id),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnVideowallManager<T>::sendControlMessage(
    const nx::vms::api::VideowallControlMessageData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::videowallControl,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
