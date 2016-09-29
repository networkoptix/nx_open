#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiLicenseData: ApiData
{
    QnLatin1Array key;
    QnLatin1Array licenseBlock;

    bool operator<(const ApiLicenseData& other) const;
};
#define ApiLicenseData_Fields (key)(licenseBlock)

struct ApiDetailedLicenseData: ApiLicenseData
{
    QString name;
    qint32 cameraCount;
    QString hardwareId;
    QString licenseType;
    QString version;
    QString brand;
    QString expiration;
};
#define ApiDetailedLicenseData_Fields ApiLicenseData_Fields \
    (name)(cameraCount)(hardwareId)(licenseType)(version)(brand)(expiration)

} // namespace ec2
