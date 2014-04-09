/**********************************************************
* 24 jan 2014
* a.kolesnikov
***********************************************************/

#include "ec2_lib.h"

#include "connection_factory.h"


extern "C"
{
    ec2::AbstractECConnectionFactory* getConnectionFactory()
    {
        return new ec2::Ec2DirectConnectionFactory();
    }
}
