#include "videowall_manager.h"

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
    QnVideowallManager<QueryProcessorType>::QnVideowallManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx )
        :
        m_queryProcessor( queryProcessor ),
        m_resCtx(resCtx)
    {
    }


    template<class T>
    int QnVideowallManager<T>::getVideowalls( impl::GetVideowallsHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiVideowallList& videowalls) {
            QnVideoWallResourceList outData;
            if( errorCode == ErrorCode::ok )
                videowalls.toResourceList(outData);
            handler->done( reqID, errorCode, outData );
        };
        m_queryProcessor->template processQueryAsync<nullptr_t, ApiVideowallList, decltype(queryDoneHandler)> ( ApiCommand::getVideowallList, nullptr, queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::save( const QnVideoWallResourcePtr& resource, impl::AddVideowallHandlerPtr handler )
    {
        //preparing output data
        QnVideoWallResourceList videowalls;
        if (resource->getId().isNull()) {
            resource->setId(QnId::createUuid());
        }
        videowalls.push_back( resource );

        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::saveVideowall, resource );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::AddVideowallHandler::done, handler, reqID, _1, videowalls ) );
        return reqID;
    }

    template<class T>
    int QnVideowallManager<T>::remove( const QnId& id, impl::SimpleHandlerPtr handler )
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
    int QnVideowallManager<T>::sendInstanceId(const QnId& id, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
      /*  auto tran = prepareTransaction( ApiCommand::removeVideowall, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( &impl::SimpleHandler::done, handler, reqID, _1 ) );*/
        return reqID;
    }

    template<class T>
    QnTransaction<ApiVideowall> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const QnVideoWallResourcePtr &resource)
    {
        QnTransaction<ApiVideowall> tran(command, true);
        tran.params.fromResource( resource );
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const QnId &id)
    {
        QnTransaction<ApiIdData> tran(command, false);
        tran.params.id = id;
        return tran;
    }

    template<class T>
    QnTransaction<ApiVideowallControlMessage> QnVideowallManager<T>::prepareTransaction(ApiCommand::Value command, const QnVideoWallControlMessage &message)
    {
        QnTransaction<ApiVideowallControlMessage> tran(command, false);
        tran.params.fromMessage(message);
        return tran;
    }

    template class QnVideowallManager<ServerQueryProcessor>;
    template class QnVideowallManager<FixedUrlClientQueryProcessor>;

}
