
#include "ec2_lib.h"

#include "connection_factory.h"


ec2::AbstractECConnectionFactory* getConnectionFactory()
{
    return new ec2::Ec2DirectConnectionFactory();
}
