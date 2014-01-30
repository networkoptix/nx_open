
#include "resource_manager.h"
#include <QtConcurrent>
#include "database/db_manager.h"

#include "client_query_processor.h"
#include "server_query_processor.h"


namespace ec2
{
    template<class T>
    QnResourceManager<T>::QnResourceManager( T* const queryProcessor )
    :
        m_queryProcessor( queryProcessor )
    {
    }

    template<class T>
    ReqID QnResourceManager<T>::getResourceTypes( impl::GetResourceTypesHandlerPtr handler )
    {
        auto queryDoneHandler = [handler]( ErrorCode errorCode, const ApiResourceTypeList& resTypeList ) {
            QnResourceTypeList outResTypeList;
            if( errorCode == ErrorCode::ok )
				resTypeList.toResourceTypeList(outResTypeList);
            handler->done( errorCode, outResTypeList );
        };
        m_queryProcessor->processQueryAsync<nullptr_t, ApiResourceTypeList, decltype(queryDoneHandler)>( nullptr, queryDoneHandler );
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::getResources( impl::GetResourcesHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::getResource( const QnId& id, impl::GetResourceHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::getKvPairs( const QnResourcePtr &resource, impl::GetKvPairsHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::setResourceDisabled( const QnId& resourceId, bool disabled, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::save( const QnResourcePtr &resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::save( int resourceId, const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    template<class T>
    ReqID QnResourceManager<T>::remove( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }



    template class QnResourceManager<ServerQueryProcessor>;
    template class QnResourceManager<ClientQueryProcessor>;
}
