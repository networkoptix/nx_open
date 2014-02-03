
#ifndef EC2_CONNECTION_DATA_H
#define EC2_CONNECTION_DATA_H

#include "api_data.h"


namespace ec2
{
    //!Response to connect request
    struct ConnectionInfo
    :
        public ApiData
    {
    };

    //!Parameters of connect request
    struct LoginInfo
    :
        public ApiData
    {
    };
}

#endif  //EC2_CONNECTION_DATA_H
