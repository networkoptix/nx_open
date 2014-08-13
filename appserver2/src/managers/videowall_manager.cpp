#include "videowall_manager.h"

#include <functional>

#include <QtConcurrent>

#include "fixed_url_client_query_processor.h"
#include "database/db_manager.h"
#include "transaction/transaction_log.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class QueryProcessorType>
    QnVideowallManager<QueryProcessorType>::QnVideowallManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
    :
        QnVideowallNotificationManager( resCtx ),
        m_queryProcessor( queryProcessor )
    {
    }


    template<class T>
    int QnVideowallManager<T>::getVideowalls( impl::GetVideowallsHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiVideowallDataList& videowalls) {
            QnVideoWallResourceList outData;
            if( errorCode == ErrorCode::ok )
                fromApiToResourceList(videowalls, outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiVideowallDataList, decltype(queryDoneHandler)> ( ApiCommand::getVideowalls, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::save( const QnVideoWallResourcePtr& resource, impl::AddVideowallHandlerPtr handler )
    {
        //preparing output data
        QnVideoWallResourceList videowalls;
        if (resource->getId().isNull()) {
            resource->setId(QUuid::createUuid());
        }
        videowalls.push_back( resource );

        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveVideowall, resource );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::AddVideowallHandler::done, handler, reqID, _1, videowalls ) );
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::remove( const QUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeVideowall, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::sendControlMessage(const QnVideoWallControlMessage& message, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::videowallControl, message );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    QnTransaction<ApiVideowallData> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const QnVideoWallResourcePtr &resource)
    {
        QnTransaction<ApiVideowallData> tran(command);
        fromResourceToApi( resource, tran.params );
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const QUuid &id)
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    template<class T>
    QnTransaction<ApiVideowallControlMessageData> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const QnVideoWallControlMessage &message)
    {
        QnTransaction<ApiVideowallControlMessageData> tran(command);
        fromResourceToApi(message, tran.params);
        return tran;
    }

    template class QnVideowallManager<ServerQueryProcessor>;
    template class QnVideowallManager<FixedUrlClientQueryProcessor>;

}
