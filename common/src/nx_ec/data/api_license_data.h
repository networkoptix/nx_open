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

struct ApiDetailedLicenseData: ApiData
{
    QnLatin1Array key;
    QString name;
    int cameraCount;
    QString hardwareId;
    QString licenseType;
    QString version;
    QString brand;
    QString expiration;
    QnLatin1Array signature;
};
#define ApiDetailedLicenseData_Fields \
    (key)(name)(cameraCount)(hardwareId)(licenseType)(version)(brand)(expiration)(signature)

} // namespace ec2
