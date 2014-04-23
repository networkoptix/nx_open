#ifndef QN_API_DATA_I_H
#define QN_API_DATA_I_H

#include "api_types_i.h"

namespace ec2 {

    struct ApiData {
        virtual ~ApiData() {}
    };

    struct ApiIdData: ApiData {
        QnId id;
    };

#define ApiIdData_Fields (id)

} // namespace ec2

#endif // QN_API_DATA_I_H
