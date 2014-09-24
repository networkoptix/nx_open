#include "business_event_manager.h"

#include <functional>

#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"
#include "fixed_url_client_query_processor.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"
#include "nx_ec/data/api_business_rule_data.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{

template<class QueryProcessorType>
QnBusinessEventManager<QueryProcessorType>::QnBusinessEventManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
:
    QnBusinessEventNotificationManager( resCtx ),
    m_queryProcessor( queryProcessor )
{
}


template<class T>
int QnBusinessEventManager<T>::getBusinessRules( impl::GetBusinessRulesHandlerPtr handler )
{
    const int reqID = generateRequestID();

    auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiBusinessRuleDataList& rules) {
        QnBusinessEventRuleList outData;
        if( errorCode == ErrorCode::ok )
            fromApiToResourceList(rules, outData, m_resCtx.pool);
        handler->done( reqID, errorCode, outData);
    };
    m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiBusinessRuleDataList, decltype(queryDoneHandler)> ( ApiCommand::getBusinessRules, nullptr, queryDoneHandler);
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler )
{
    const int reqID = generateRequestID();

    if (rule->id().isNull())
        rule->setId(QUuid::createUuid());

    auto tran = prepareTransaction( ApiCommand::saveBusinessRule, rule );

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SaveBusinessRuleHandler::done, handler, reqID, _1, rule ) );

    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::deleteRule( QUuid ruleId, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction( ApiCommand::removeBusinessRule, ruleId );
    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction( ApiCommand::broadcastBusinessAction, businessAction );
    QnTransactionMessageBus::instance()->sendTransaction(tran);
    QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
    QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( &impl::SimpleHandler::done, handler, reqID, ErrorCode::ok ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::sendBusinessAction( const QnAbstractBusinessActionPtr& businessAction, const QUuid& dstPeer, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction( ApiCommand::execBusinessAction, businessAction );
    QnTransactionMessageBus::instance()->sendTransaction(tran, dstPeer);
    QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
    QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( &impl::SimpleHandler::done, handler, reqID, ErrorCode::ok ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::resetBusinessRules( impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    QnTransaction<ApiResetBusinessRuleData> tran(ApiCommand::resetBusinessRules);
    fromResourceListToApi(QnBusinessEventRule::getDefaultRules(), tran.params.defaultRules);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );

    return reqID;
}

template<class T>
QnTransaction<ApiBusinessActionData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnAbstractBusinessActionPtr& resource )
{
    QnTransaction<ApiBusinessActionData> tran(command);
    fromResourceToApi(resource, tran.params);
    return tran;
}


template<class T>
QnTransaction<ApiBusinessRuleData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource )
{
    QnTransaction<ApiBusinessRuleData> tran(command);
    fromResourceToApi(resource, tran.params);
    return tran;
}

template<class T>
QnTransaction<ApiIdData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QUuid& id )
{
    QnTransaction<ApiIdData> tran(command);
    tran.params.id = id;
    return tran;
}

template class QnBusinessEventManager<ServerQueryProcessor>;
template class QnBusinessEventManager<FixedUrlClientQueryProcessor>;

}
