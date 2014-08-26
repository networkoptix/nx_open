
#include "resource_manager.h"
#include <QtConcurrent>
#include "database/db_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "core/resource_management/resource_pool.h"
#include "nx_ec/ec_api.h"
#include "common/common_module.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    template<class T>
    QnResourceManager<T>::QnResourceManager( T* const queryProcessor, const ResourceContext& resCtx)
    :
        QnResourceNotificationManager( resCtx ),
        m_queryProcessor( queryProcessor )
    {
    }

    template<class T>
    int QnResourceManager<T>::getResourceTypes( impl::GetResourceTypesHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler]( ErrorCode errorCode, const ApiResourceTypeDataList& resTypeList ) {
            QnResourceTypeList outResTypeList;
            if( errorCode == ErrorCode::ok )
				fromApiToResourceList(resTypeList, outResTypeList);
            handler->done( reqID, errorCode, outResTypeList );
        };
        m_queryProcessor->template processQueryAsync<std::nullptr_t, ApiResourceTypeDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceTypes, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::setResourceStatus( const QUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //performing request
        auto tran = prepareTransaction( ApiCommand::setResourceStatus, resourceId, status );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SetResourceStatusHandler::done ), handler, reqID, _1, resourceId));
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::getKvPairs( const QUuid &resourceId, impl::GetKvPairsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        
        auto queryDoneHandler = [reqID, handler, resourceId]( ErrorCode errorCode, const ApiResourceParamsData& params) {
            QnKvPairListsById outData;
            outData.insert(resourceId, QnKvPairList());
            if( errorCode == ErrorCode::ok ) {
                QnKvPairList& outParams = outData.begin().value();
                foreach(const ApiResourceParamData& param, params.params)
                    outParams << QnKvPair(param.name, param.value);
            }
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<QUuid, ApiResourceParamsData, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceParams, resourceId, queryDoneHandler );
        return reqID;
    }

    /*
    template<class T>
    int QnResourceManager<T>::setResourceDisabled( const QUuid& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //performing request
        auto tran = prepareTransaction( ApiCommand::setResourceDisabled, resourceId, disabled );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SetResourceDisabledHandler::done ), handler, reqID, _1, resourceId));
        return reqID;
    }
    */


    template<class T>
    int QnResourceManager<T>::save( const QUuid& resourceId, const QnKvPairList& kvPairs, bool isPredefinedParams, impl::SaveKvPairsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiCommand::Value command = ApiCommand::setResourceParams;
        auto tran = prepareTransaction( command, resourceId, kvPairs, isPredefinedParams );
        QnKvPairListsById outData;
        outData.insert(resourceId, kvPairs);
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SaveKvPairsHandler::done ), handler, reqID, _1, outData) );

        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::remove( const QUuid& id, impl::SimpleHandlerPtr handler )
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
        const QUuid& id, Qn::ResourceStatus status)
    {
        QnTransaction<ApiSetResourceStatusData> tran(command);
        tran.params.id = id;
        tran.params.status = status;
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiResourceParamsData> QnResourceManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const QUuid& id, const QnKvPairList& kvPairs, bool isPredefinedParams)
    {
        QnTransaction<ApiResourceParamsData> tran(command);
        tran.params.params.reserve(kvPairs.size());
        foreach(const QnKvPair& pair, kvPairs)
            tran.params.params.push_back(ApiResourceParamData(pair.name(), pair.value(), isPredefinedParams));
        tran.params.id = id;
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        
        if (command == ApiCommand::setResourceStatus) 
        {
            QnResourcePtr res = m_resCtx.pool->getResourceById(id);
            if (id == qnCommon->moduleGUID())
                tran.isLocal = true;
            else if (res && res->hasFlags(Qn::foreigner))
                tran.isLocal = true;
        }

        return tran;
    }

    /*
    template<class T>
    QnTransaction<ApiSetResourceDisabledData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QUuid& id, bool disabled )
    {
        QnTransaction<ApiSetResourceDisabledData> tran(command, true);
        tran.params.id = id;
        tran.params.disabled = disabled;
        return tran;
    }
    */

    template<class T>
    QnTransaction<ApiResourceData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QnResourcePtr& resource )
    {
        QnTransaction<ApiResourceData> tran(command);
        fromResourceToApi(resource, tran.params);
        return tran;
    }

    template class QnResourceManager<ServerQueryProcessor>;
    template class QnResourceManager<FixedUrlClientQueryProcessor>;
}
