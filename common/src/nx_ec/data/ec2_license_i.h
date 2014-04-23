#ifndef QN_LICENSE_I_H
#define QN_LICENSE_I_H

#include "api_data_i.h"

namespace ec2 {
    
    struct ApiLicenseData : ApiData {
        QByteArray key;
        QByteArray licenseBlock;
    };

#define ApiLicenseData_Fields (key)(licenseBlock)

} // namespace ec2

#endif // QN_LICENSE_I_H
