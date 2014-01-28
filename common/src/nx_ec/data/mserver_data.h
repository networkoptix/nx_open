
#ifndef MSERVER_DATA_H
#define MSERVER_DATA_H

#include "resource_data.h"
#include "serialization_helper.h"


namespace ec2
{
    struct ApiMediaServerData: public ApiResourceData
    {
        //TODO/IMPL

        //QN_DECLARE_STRUCT_SERIALIZATORS();
    };
}

//QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS (ec2::ApiMediaServerData, ApiResourceData, )

#endif //MSERVER_DATA_H
