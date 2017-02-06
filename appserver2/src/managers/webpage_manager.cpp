#include "webpage_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2
{
    QnWebPageNotificationManager::QnWebPageNotificationManager()
    {}

    void QnWebPageNotificationManager::triggerNotification(const QnTransaction<ApiWebPageData> &tran, NotificationSource source)
    {
        NX_ASSERT(tran.command == ApiCommand::saveWebPage);
        emit addedOrUpdated(tran.params, source);
    }

    void QnWebPageNotificationManager::triggerNotification(const QnTransaction<ApiIdData> &tran, NotificationSource /*source*/)
    {
        NX_ASSERT(tran.command == ApiCommand::removeWebPage );
        emit removed( QnUuid(tran.params.id) );
    }

    template<class QueryProcessorType>
    QnWebPageManager<QueryProcessorType>::QnWebPageManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData)
        : m_queryProcessor(queryProcessor),
          m_userAccessData(userAccessData)
    {}

    template<class QueryProcessorType>
    int QnWebPageManager<QueryProcessorType>::getWebPages( impl::GetWebPagesHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler](ErrorCode errorCode, const ec2::ApiWebPageDataList& webpages)
        {
            handler->done(reqID, errorCode, webpages);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiWebPageDataList, decltype(queryDoneHandler)> ( ApiCommand::getWebPages, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class QueryProcessorType>
    int QnWebPageManager<QueryProcessorType>::save( const ec2::ApiWebPageData& webpage, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveWebPage, webpage,
            [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
        return reqID;
    }

    template<class QueryProcessorType>
    int QnWebPageManager<QueryProcessorType>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeWebPage, ApiIdData(id),
            [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
        return reqID;
    }

    template class QnWebPageManager<ServerQueryProcessorAccess>;
    template class QnWebPageManager<FixedUrlClientQueryProcessor>;
}


