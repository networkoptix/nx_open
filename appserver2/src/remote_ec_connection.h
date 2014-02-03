/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef REMOTE_EC_CONNECTION_H
#define REMOTE_EC_CONNECTION_H

#include <nx_ec/ec_api.h>

#include "base_ec2_connection.h"
#include "client_query_processor.h"


namespace ec2
{
    class RemoteEC2Connection
    :
        public BaseEc2Connection<ClientQueryProcessor>
    {
    public:
        RemoteEC2Connection( ClientQueryProcessor* queryProcessor, const QnResourceFactoryPtr& resourceFactory , QnResourcePool* resPool);
    };
}

#endif  //REMOTE_EC_CONNECTION_H
