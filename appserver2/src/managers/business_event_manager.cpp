#include "business_event_manager.h"

#include <functional>
#include <QtConcurrent>

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
    m_queryProcessor( queryProcessor ),
    m_resCtx( resCtx )
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
    m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiBusinessRuleDataList, decltype(queryDoneHandler)> ( ApiCommand::getBusinessRuleList, nullptr, queryDoneHandler);
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::testEmailSettings( const QnEmail::Settings& settings, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();

    auto tran = prepareTransaction( ApiCommand::testEmailSettings, settings );

    m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, std::placeholders::_1 ) );
    
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::sendEmail(const ApiEmailData& data, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();

    auto tran = prepareTransaction( ApiCommand::sendEmail, data );

    m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, std::placeholders::_1 ) );
    
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler )
{
    const int reqID = generateRequestID();

    if (rule->id().isNull())
        rule->setId(QnId::createUuid());

    auto tran = prepareTransaction( ApiCommand::saveBusinessRule, rule );

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SaveBusinessRuleHandler::done, handler, reqID, _1, rule ) );

    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::deleteRule( QnId ruleId, impl::SimpleHandlerPtr handler )
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
    QnScopedThreadRollback ensureFreeThread(1);
    QtConcurrent::run( std::bind( &impl::SimpleHandler::done, handler, reqID, ErrorCode::ok ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::sendBusinessAction( const QnAbstractBusinessActionPtr& businessAction, const QnId& dstPeer, impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    auto tran = prepareTransaction( ApiCommand::execBusinessAction, businessAction );
    QnTransactionMessageBus::instance()->sendTransaction(tran, dstPeer);
    QnScopedThreadRollback ensureFreeThread(1);
    QtConcurrent::run( std::bind( &impl::SimpleHandler::done, handler, reqID, ErrorCode::ok ) );
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::resetBusinessRules( impl::SimpleHandlerPtr handler )
{
    const int reqID = generateRequestID();
    QnTransaction<ApiResetBusinessRuleData> tran(ApiCommand::resetBusinessRules, true);
    fromResourceListToApi(QnBusinessEventRule::getDefaultRules(), tran.params.defaultRules);

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );

    return reqID;
}

template<class T>
QnTransaction<ApiBusinessActionData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnAbstractBusinessActionPtr& resource )
{
    QnTransaction<ApiBusinessActionData> tran(command, false);
    fromResourceToApi(resource, tran.params);
    return tran;
}


template<class T>
QnTransaction<ApiBusinessRuleData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource )
{
    QnTransaction<ApiBusinessRuleData> tran(command, true);
    fromResourceToApi(resource, tran.params);
    return tran;
}

template<class T>
QnTransaction<ApiEmailSettingsData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnEmail::Settings& resource )
{
    QnTransaction<ApiEmailSettingsData> tran(command, true);
    fromResourceToApi(resource, tran.params);
    tran.persistent = false;
    tran.localTransaction = true;
    return tran;
}

template<class T>
QnTransaction<ApiEmailData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const ApiEmailData& data )
{
    QnTransaction<ApiEmailData> tran(command, true);
    tran.params = data;
    tran.persistent = false;
    tran.localTransaction = true;
    return tran;
}

template<class T>
QnTransaction<ApiIdData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnId& id )
{
    QnTransaction<ApiIdData> tran(command, true);
    tran.params.id = id;
    return tran;
}

template class QnBusinessEventManager<ServerQueryProcessor>;
template class QnBusinessEventManager<FixedUrlClientQueryProcessor>;

}
