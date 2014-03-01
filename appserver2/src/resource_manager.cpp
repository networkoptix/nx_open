
#include "resource_manager.h"
#include <QtConcurrent>
#include "database/db_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
#include "nx_ec/ec_api.h"


namespace ec2
{
    template<class T>
    QnResourceManager<T>::QnResourceManager( T* const queryProcessor, const ResourceContext& resCtx)
    :
        m_queryProcessor( queryProcessor ),
        m_resCtx( resCtx )
    {
    }

    template<class T>
    int QnResourceManager<T>::getResourceTypes( impl::GetResourceTypesHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiResourceTypeList& resTypeList ) {
            QnResourceTypeList outResTypeList;
            if( errorCode == ErrorCode::ok )
				resTypeList.toResourceTypeList(outResTypeList);
            handler->done( reqID, errorCode, outResTypeList );
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiResourceTypeList, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceTypes, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SetResourceStatusHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //performing request
        auto tran = prepareTransaction( ApiCommand::setResourceStatus, resourceId, status );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SetResourceStatusHandler::done ), handler, reqID, _1, resourceId));
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::getKvPairs( const QnId &resourceId, impl::GetKvPairsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        
        auto queryDoneHandler = [reqID, handler, resourceId]( ErrorCode errorCode, const ApiResourceParams& params) {
            QnKvPairListsById outData;
            outData.insert(resourceId, QnKvPairList());
            if( errorCode == ErrorCode::ok ) {
                QnKvPairList& outParams = outData.begin().value();
                foreach(const ApiResourceParam& param, params.params)
                    outParams << QnKvPair(param.name, param.value);
            }
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<QnId, ApiResourceParams, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceParams, resourceId, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::setResourceDisabled( const QnId& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //performing request
        auto tran = prepareTransaction( ApiCommand::setResourceDisabled, resourceId, disabled );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SetResourceDisabledHandler::done ), handler, reqID, _1, resourceId));
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::save( const QnResourcePtr &resource, impl::SaveResourceHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        if (resource->getId().isNull()) {
            Q_ASSERT_X(0, "Only UPDATE operation is supported for saving resource!", Q_FUNC_INFO);
            return INVALID_REQ_ID;
        }

        //performing request
        auto tran = prepareTransaction( ApiCommand::saveResource, resource );

        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SaveResourceHandler::done ), handler, reqID, _1, resource ) );

        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::save( const QnId& resourceId, const QnKvPairList& kvPairs, impl::SaveKvPairsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiCommand::Value command = ApiCommand::setResourceParams;
        auto tran = prepareTransaction( command, resourceId, kvPairs );
        QnKvPairListsById outData;
        outData.insert(resourceId, kvPairs);
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SaveKvPairsHandler::done ), handler, reqID, _1, outData) );

        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::remove( const QnId& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto tran = prepareTransaction( ApiCommand::removeResource, id );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiSetResourceStatusData> QnResourceManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnId& id, QnResource::Status status)
    {
        QnTransaction<ApiSetResourceStatusData> tran(command, true);
        tran.params.id = id;
        tran.params.status = status;
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiResourceParams> QnResourceManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QnId& id, const QnKvPairList& kvPairs)
    {
        QnTransaction<ApiResourceParams> tran(command, true);
        tran.params.params.reserve(kvPairs.size());
        foreach(const QnKvPair& pair, kvPairs)
            tran.params.params.push_back(ApiResourceParam(pair.name(), pair.value()));
        tran.params.id = id;
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QnId& id )
    {
        QnTransaction<ApiIdData> tran(command, true);
        tran.params.id = id;
        
        if (command == ApiCommand::setResourceStatus) {
            QnResourcePtr mServer = m_resCtx.pool->getResourceById(id).dynamicCast<QnMediaServerResource>();
            if (mServer)
                tran.localTransaction = true;
        }

        return tran;
    }

    template<class T>
    QnTransaction<ApiSetResourceDisabledData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QnId& id, bool disabled )
    {
        QnTransaction<ApiSetResourceDisabledData> tran(command, true);
        tran.params.id = id;
        tran.params.disabled = disabled;
        return tran;
    }

    template<class T>
    QnTransaction<ApiResourceData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QnResourcePtr& resource )
    {
        QnTransaction<ApiResourceData> tran(command, true);
        tran.params.fromResource(resource);
        return tran;
    }

    template class QnResourceManager<ServerQueryProcessor>;
    template class QnResourceManager<FixedUrlClientQueryProcessor>;
}
