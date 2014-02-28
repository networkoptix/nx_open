#include "business_event_manager.h"

#include <functional>
#include <QtConcurrent>

#include "fixed_url_client_query_processor.h"
#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"

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
            rules.toResourceList(outData, m_resCtx.pool);
        handler->done( reqID, errorCode, outData);
    };
    m_queryProcessor->processQueryAsync<nullptr_t, ApiBusinessRuleDataList, decltype(queryDoneHandler)> ( ApiCommand::getBusinessRuleList, nullptr, queryDoneHandler);
    return reqID;
}

template<class T>
int QnBusinessEventManager<T>::testEmailSettings( const QnKvPairList& /*settings*/, impl::SimpleHandlerPtr /*handler*/ )
{
    //Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
int QnBusinessEventManager<T>::sendEmail(const QStringList& to, const QString& subject, const QString& message, int timeout, const QnEmailAttachmentList& attachments, impl::SimpleHandlerPtr handler )
{
    //Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
int QnBusinessEventManager<T>::save( const QnBusinessEventRulePtr& rule, impl::SaveBusinessRuleHandlerPtr handler )
{
    const int reqID = generateRequestID();

    if (rule->id().isNull())
        rule->setId(QnId::createUuid());

    auto tran = prepareTransaction( ApiCommand::saveBusinessRule, rule );

    using namespace std::placeholders;
    m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SaveBusinessRuleHandler::done ), handler, reqID, _1, rule ) );

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
    auto tran = prepareTransaction( ApiCommand::broadcastBusinessAction, businessAction );
    QnTransactionMessageBus::instance()->sendTransaction(tran);
    return INVALID_REQ_ID;
}

template<class T>
int QnBusinessEventManager<T>::resetBusinessRules( impl::SimpleHandlerPtr handler )
{
    //Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
QnTransaction<ApiBusinessActionData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnAbstractBusinessActionPtr& resource )
{
    QnTransaction<ApiBusinessActionData> tran(command, false);
    tran.params.fromResource(resource);
    return tran;
}


template<class T>
QnTransaction<ApiBusinessRuleData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource )
{
    QnTransaction<ApiBusinessRuleData> tran(command, true);
    tran.params.fromResource(resource);
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
