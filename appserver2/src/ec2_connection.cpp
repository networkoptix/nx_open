/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_connection.h"


namespace ec2
{
    Ec2DirectConnection::Ec2DirectConnection( ServerQueryProcessor* queryProcessor, const QnResourceFactoryPtr& resourceFactory )
    :
        BaseEc2Connection<ServerQueryProcessor>( queryProcessor, resourceFactory ),
        m_dbManager( new QnDbManager(resourceFactory) )
    {
    }
}
