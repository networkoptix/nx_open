
#include "resource_manager.h"
#include <QtConcurrent>
#include "database/db_manager.h"


namespace ec2
{
    ReqID QnResourceManager::getResourceTypes( impl::GetResourceTypesHandlerPtr handler )
    {
		
		ApiResourceTypeList resTypes;
		dbManager->getResourceTypes(resTypes);

		QnResourceTypeList result;
		ErrorCode errorCode;
		QtConcurrent::run( std::bind( std::mem_fn( &impl::GetResourceTypesHandler::done ), handler, errorCode, result ) );
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::getResources( impl::GetResourcesHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::getResource( const QnId& id, impl::GetResourceHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::getKvPairs( const QnResourcePtr &resource, impl::GetKvPairsHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::setResourceDisabled( const QnId& resourceId, bool disabled, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::save( const QnResourcePtr &resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::save( int resourceId, const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }

    ReqID QnResourceManager::remove( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler )
    {
        //TODO/IMPL
        return INVALID_REQ_ID;
    }
}
