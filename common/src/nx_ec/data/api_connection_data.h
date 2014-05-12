#ifndef EC2_CONNECTION_DATA_H
#define EC2_CONNECTION_DATA_H

#include "api_data.h"

namespace ec2
{
    // TODO: #EC2 #Elric doesn't conform to API naming conventions.
    // And what it is, anyway? Why just an empty struct?

    //!Parameters of connect request
    struct LoginInfo: ApiData 
    {
    };
}

#endif  //EC2_CONNECTION_DATA_H
