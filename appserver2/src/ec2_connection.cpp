/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"


namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection( ServerQueryProcessor* queryProcessor, const QnResourceFactoryPtr& resourceFactory, QnResourcePool* resPool )
    :
        BaseEc2Connection<ServerQueryProcessor>( queryProcessor, resourceFactory, resPool ),
        m_dbManager( new QnDbManager(resourceFactory) )
    {
    }
}
