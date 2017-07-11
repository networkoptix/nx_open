
#include "resource_manager.h"
#include <QtConcurrent/QtConcurrent>
#include <database/db_manager.h>

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
    QnResourceManager<T>::QnResourceManager( T* const queryProcessor, const Qn::UserAccessData &userAccessData)
    :
        m_queryProcessor( queryProcessor ),
        m_userAccessData(userAccessData)
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
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<std::nullptr_t, ApiResourceTypeDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceTypes, nullptr, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiResourceStatusData params;
        params.id = resourceId;
        params.status = status;

        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::setResourceStatus, params,
            std::bind( std::mem_fn( &impl::SetResourceStatusHandler::done ), handler, reqID, _1, resourceId));
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
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiResourceParamWithRefDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getResourceParams, resourceId, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::getStatusList( const QnUuid &resourceId, impl::GetStatusListHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, resourceId]( ErrorCode errorCode, const ApiResourceStatusDataList& params) {
            ApiResourceStatusDataList outData;
            if( errorCode == ErrorCode::ok )
                outData = params;
            handler->done( reqID, errorCode, outData);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiResourceStatusDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getStatusList, resourceId, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::setResourceParams, kvPairs,
            std::bind(std::mem_fn(&impl::SimpleHandler::done), handler, reqID, _1));

        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        using namespace std::placeholders;

        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeResource, ApiIdData(id),
            std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template<class T>
    int QnResourceManager<T>::remove( const QVector<QnUuid>& idList, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        ApiIdDataList params;
        for(const QnUuid& id: idList)
            params.push_back(id);
        using namespace std::placeholders;
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeResources, params,
            std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, reqID, _1 ) );
        return reqID;
    }

    template class QnResourceManager<ServerQueryProcessorAccess>;
    template class QnResourceManager<FixedUrlClientQueryProcessor>;
}
