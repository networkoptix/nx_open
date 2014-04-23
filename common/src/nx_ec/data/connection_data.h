
#ifndef EC2_CONNECTION_DATA_H
#define EC2_CONNECTION_DATA_H

#include "api_data.h"

#include <api/model/connection_info.h>
#include <utils/common/request_param.h>


namespace ec2
{
    //!Parameters of connect request
    struct LoginInfo
    :
        public ApiData
    {
    };

    inline void parseHttpRequestParams( const QnRequestParamList& /*params*/, ec2::LoginInfo* /*loginInfo*/ )
    {
        //TODO/IMPL
    }
}

#endif  //EC2_CONNECTION_DATA_H
