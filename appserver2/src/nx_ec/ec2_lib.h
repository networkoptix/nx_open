
#ifndef EC2_LIB_H
#define EC2_LIB_H

#include "ec_api.h"


extern "C"
{
    EC2_LIB_API ec2::AbstractECConnectionFactory* getConnectionFactory();
}

#endif  //EC2_LIB_H
