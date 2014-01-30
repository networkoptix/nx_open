/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#include "remote_ec_connection.h"


namespace ec2
{
    RemoteEC2Connection::RemoteEC2Connection( ClientQueryProcessor* queryProcessor, const QnResourceFactoryPtr& resourceFactory )
    :
        BaseEc2Connection<ClientQueryProcessor>( queryProcessor, resourceFactory )
    {
    }
}
