#ifndef QN_API_DATA_H
#define QN_API_DATA_H

#include "api_globals.h"

namespace ec2 {
    
    struct ApiData {
        virtual ~ApiData() {}
    };

    struct ApiIdData: ApiData {
        QnId id;
    };
#define ApiIdData_Fields (id)

} // namespace ec2


#endif // QN_API_DATA_H
