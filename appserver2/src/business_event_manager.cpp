#include "business_event_manager.h"

#include <functional>
#include <QtConcurrent>

#include "client_query_processor.h"
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
ReqID QnBusinessEventManager<T>::getBusinessRules( impl::GetBusinessRulesHandlerPtr handler )
{
    auto queryDoneHandler = [handler, this]( ErrorCode errorCode, const ApiBusinessRuleDataList& rules) {
        QnBusinessEventRuleList outData;
        if( errorCode == ErrorCode::ok )
            rules.toResourceList(outData, m_resCtx.pool);
        handler->done( errorCode, outData);
    };
    m_queryProcessor->processQueryAsync<nullptr_t, ApiBusinessRuleDataList, decltype(queryDoneHandler)> ( ApiCommand::getBusinessRuleList, nullptr, queryDoneHandler);
    return INVALID_REQ_ID;
}

template<class T>
ReqID QnBusinessEventManager<T>::addBusinessRule( const QnBusinessEventRulePtr& businessRule, impl::SimpleHandlerPtr handler )
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
ReqID QnBusinessEventManager<T>::testEmailSettings( const QnKvPairList& settings, impl::SimpleHandlerPtr handler )
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
ReqID QnBusinessEventManager<T>::sendEmail(const QStringList& to, const QString& subject, const QString& message, int timeout, const QnEmailAttachmentList& attachments, impl::SimpleHandlerPtr handler )
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
ReqID QnBusinessEventManager<T>::save( const QnBusinessEventRulePtr& rule, impl::SimpleHandlerPtr handler )
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
ReqID QnBusinessEventManager<T>::deleteRule( int ruleId, impl::SimpleHandlerPtr handler )
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
ReqID QnBusinessEventManager<T>::broadcastBusinessAction( const QnAbstractBusinessActionPtr& businessAction, impl::SimpleHandlerPtr handler )
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
ReqID QnBusinessEventManager<T>::resetBusinessRules( impl::SimpleHandlerPtr handler )
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "todo: implement me!!!");
    return INVALID_REQ_ID;
}

template<class T>
QnTransaction<ApiBusinessRuleData> QnBusinessEventManager<T>::prepareTransaction( ApiCommand::Value command, const QnBusinessEventRulePtr& resource )
{
    QnTransaction<ApiBusinessRuleData> tran;
    tran.createNewID();
    tran.command = command;
    tran.persistent = true;
    //TODO/IMPL
    return tran;
}

template class QnBusinessEventManager<ServerQueryProcessor>;
template class QnBusinessEventManager<ClientQueryProcessor>;

}
