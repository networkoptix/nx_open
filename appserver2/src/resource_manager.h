
#ifndef EC2_RESOURCE_MANAGER_H
#define EC2_RESOURCE_MANAGER_H

#include "nx_ec/ec_api.h"


namespace ec2
{

    class ResourceManager
    :
        public AbstractResourceManager
    {
    public:
        virtual ReqID getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) override;
        virtual ReqID getResources( impl::GetResourcesHandlerPtr handler ) override;
        virtual ReqID getResource( const QnId& id, impl::GetResourceHandlerPtr handler ) override;
        virtual ReqID setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SimpleHandlerPtr handler ) override;

        virtual ReqID getKvPairs( const QnResourcePtr &resource, impl::GetKvPairsHandlerPtr handler ) override;
        virtual ReqID setResourceDisabled( const QnId& resourceId, bool disabled, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID save( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID save( int resourceId, const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) override;
        virtual ReqID remove( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
    };

}

#endif  //EC2_RESOURCE_MANAGER_H
