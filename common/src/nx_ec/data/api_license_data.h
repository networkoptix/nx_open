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
        QnLatin1Array key;
        QnLatin1Array licenseBlock;
    };
#define ApiLicenseData_Fields (key)(licenseBlock)

    struct ApiDetailedLicenseData: public ApiLicenseData
    {
        QString name;
        qint32 cameraCount;
        QByteArray hardwareId;
        QString licenseType;
        QString version;
        QString brand;
        QString expiration;
    };
#define ApiDetailedLicenseData_Fields ApiLicenseData_Fields (name)(cameraCount)(hardwareId)(licenseType)(version)(brand)(expiration)
}

#endif  //API_LICENSE_DATA_H
