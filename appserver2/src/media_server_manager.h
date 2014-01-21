
#ifndef MEDIA_SERVER_MANAGER_H
#define MEDIA_SERVER_MANAGER_H

#include "nx_ec/ec_api.h"

namespace ec2
{

class QnMediaServerManager
:
    public AbstractMediaServerManager
{
public:
    virtual ReqID getServers( impl::GetServersHandlerPtr handler ) override;
    virtual ReqID save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
    virtual ReqID saveServer( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) override;
    virtual ReqID remove( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
};


}

#endif  //MEDIA_SERVER_MANAGER_H
