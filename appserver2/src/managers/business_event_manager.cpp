#include "business_event_manager.h"

#include <functional>

#include <nx/utils/concurrent.h>

#include "ec2_thread_pool.h"
#include "fixed_url_client_query_processor.h"
#include <database/db_manager.h>
#include "transaction/transaction_log.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include <transaction/message_bus_adapter.h>

using namespace nx;

namespace ec2
{

template<class QueryProcessorType>
QnBusinessEventManager<QueryProcessorType>::QnBusinessEventManager(
    TransactionMessageBusAdapter* messageBus,
    QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData &userAccessData)
:
    m_messageBus(messageBus),
    m_queryProcessor( queryProcessor ),
    m_userAccessData(userAccessData)
{
}


template<class T>
int QnBusinessEventManager<T>::getBusinessRules( impl::GetBusinessRulesHandlerPtr handler )
{
    const int reqID = generateRequestID();

    auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiBusinessRuleDataList& rules)
    {
        vms::event::RuleList outData;
        if( errorCode == ErrorCode::ok )
            fromApiToResourceList(rules, outData);
        handler->done( reqID, errorCode, outData);
    };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<
        QnUuid,
        ApiBusinessRuleDataList,
        decltype(queryDoneHandler)> ( ApiCommand::getEventRules, QnUuid(), queryDoneHandler);
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::save( const vms::event::RulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    if (rule->id().isNull())
        rule->setId(QnUuid::createUuid());

    ApiBusinessRuleData params;
    fromResourceToApi(rule, params);

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveEventRule, params,
        std::bind( &impl::SaveBusinessRuleHandler::done, handler, reqID, _1, rule ) );

    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::deleteRule( QnUuid ruleId, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeEventRule, ApiIdData(ruleId),
        std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::broadcastBusinessAction( const vms::event::AbstractActionPtr& businessAction, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction( ApiCommand::broadcastAction, businessAction );
    m_messageBus->sendTransaction(tran);
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind( &impl::SimpleHandler::done, handler, reqID, ErrorCode::ok ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::sendBusinessAction( const vms::event::AbstractActionPtr& businessAction, const QnUuid& dstPeer, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction( ApiCommand::execAction, businessAction );
    m_messageBus->sendTransaction(tran, dstPeer);
    nx::utils::concurrent::run(
        Ec2ThreadPool::instance(),
        std::bind( &impl::SimpleHandler::done, handler, reqID, ErrorCode::ok ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::resetBusinessRules( impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    ApiResetBusinessRuleData params;
    // providing event rules set from the client side is rather incorrect way of reset.

    using namespace std::placeholders;
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::resetEventRules, params,
        std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );

    return reqID;
}

template<class T>
QnTransaction<ApiBusinessActionData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const vms::event::AbstractActionPtr& resource )
{
    QnTransaction<ApiBusinessActionData> tran(command, m_messageBus->commonModule()->moduleGUID());
    fromResourceToApi(resource, tran.params);
    return tran;
}

template class QnBusinessEventManager<ServerQueryProcessorAccess>;
template class QnBusinessEventManager<FixedUrlClientQueryProcessor>;

}
