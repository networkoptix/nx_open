/**********************************************************
* 4 mar 2014
* a.kolesnikov
***********************************************************/

#ifndef API_LICENSE_DATA_H
#define API_LICENSE_DATA_H

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{
    struct ApiLicenseData : ApiData {
        QByteArray key;
        QByteArray licenseBlock;
    };
#define ApiLicenseData_Fields (key)(licenseBlock)

}

#endif  //API_LICENSE_DATA_H
