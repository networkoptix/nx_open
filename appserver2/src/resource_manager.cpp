
#include "resource_manager.h"
#include <QtConcurrent>
#include "database/db_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"


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
    int QnResourceManager<T>::getResources( impl::GetResourcesHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    int QnResourceManager<T>::getResource( const QnId& id, impl::GetResourceHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
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
                foreach(const ApiResourceParam& param, params)
                    outParams << QnKvPair(param.name, param.value);
            }
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->processQueryAsync<QnId, ApiResourceParams, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceParams, resourceId, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::setResourceDisabled( const QnId& resourceId, bool disabled, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    int QnResourceManager<T>::save( const QnResourcePtr &resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
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
    int QnResourceManager<T>::remove( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiSetResourceStatusData> QnResourceManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value cmd,
        const QnId& id, QnResource::Status status)
    {
        QnTransaction<ApiSetResourceStatusData> result;
        result.command = cmd;
        result.createNewID();
        result.persistent = true;
        result.params.id = id;
        result.params.status = status;
        return result;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiResourceParams> QnResourceManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value cmd,
        const QnId& id, const QnKvPairList& kvPairs)
    {
        QnTransaction<ApiResourceParams> result;
        result.command = cmd;
        result.createNewID();
        result.persistent = true;
        result.params.reserve(kvPairs.size());
        foreach(const QnKvPair& pair, kvPairs)
            result.params.push_back(ApiResourceParam(id, pair.name(), pair.value()));
        return result;
    }

    template class QnResourceManager<ServerQueryProcessor>;
    template class QnResourceManager<FixedUrlClientQueryProcessor>;
}
