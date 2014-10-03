
#include "resource_manager.h"
#include <QtConcurrent>
#include "database/db_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"
#include "core/resource_management/resource_pool.h"
#include "nx_ec/ec_api.h"
#include "common/common_module.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "nx_ec/data/api_resource_type_data.h"

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
    int QnResourceManager<T>::setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        //performing request
        auto tran = prepareTransaction( ApiCommand::setResourceStatus, resourceId, status );
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SetResourceStatusHandler::done ), handler, reqID, _1, resourceId));
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::getKvPairs( const QnUuid &resourceId, impl::GetKvPairsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        
        auto queryDoneHandler = [reqID, handler, resourceId]( ErrorCode errorCode, const ApiResourceParamWithRefDataList& params) {
            ApiResourceParamWithRefDataList outData;
            if( errorCode == ErrorCode::ok )
                outData = params;
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->template processQueryAsync<QnUuid, ApiResourceParamWithRefDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceParams, resourceId, queryDoneHandler );
        return reqID;
    }

    /*
    template<class T>
    int QnResourceManager<T>::setResourceDisabled( const QnUuid& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler )
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
    int QnResourceManager<T>::save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiCommand::Value command = ApiCommand::setResourceParams;
        auto tran = prepareTransaction( command, kvPairs);
        ApiResourceParamWithRefDataList outData;
        outData = kvPairs;
        using namespace std::placeholders;
        m_queryProcessor->processUpdateAsync( tran, std::bind( std::mem_fn( &impl::SaveKvPairsHandler::done ), handler, reqID, _1, outData) );

        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
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
        const QnUuid& id, Qn::ResourceStatus status)
    {
        QnTransaction<ApiSetResourceStatusData> tran(command);
        tran.params.id = id;
        tran.params.status = status;
        return tran;
    }

    template<class QueryProcessorType>
    QnTransaction<ApiResourceParamWithRefDataList> QnResourceManager<QueryProcessorType>::prepareTransaction(
        ApiCommand::Value command,
        const ec2::ApiResourceParamWithRefDataList& kvPairs)
    {
        QnTransaction<ApiResourceParamWithRefDataList> tran(command);
        tran.params = kvPairs;
        return tran;
    }

    template<class T>
    QnTransaction<ApiIdData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QnUuid& id )
    {
        QnTransaction<ApiIdData> tran(command);
        tran.params.id = id;
        return tran;
    }

    /*
    template<class T>
    QnTransaction<ApiSetResourceDisabledData> QnResourceManager<T>::prepareTransaction( ApiCommand::Value command, const QnUuid& id, bool disabled )
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
