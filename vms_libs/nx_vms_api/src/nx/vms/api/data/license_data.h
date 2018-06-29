#pragma once

#include "data.h"

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/utils/latin1_array.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API LicenseData: Data
{
    QnLatin1Array key;
    QnLatin1Array licenseBlock;

    bool operator<(const LicenseData& other) const;
};
#define LicenseData_Fields (key)(licenseBlock)

struct NX_VMS_API DetailedLicenseData: Data
{
    QnLatin1Array key;
    QString name;
    qint32 cameraCount = 0;
    QString hardwareId;
    QString licenseType;
    QString version;
    QString brand;
    QString expiration;
    QnLatin1Array signature;
};
#define DetailedLicenseData_Fields \
    (key)(name)(cameraCount)(hardwareId)(licenseType)(version)(brand)(expiration)(signature)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::LicenseData)
Q_DECLARE_METATYPE(nx::vms::api::LicenseDataList)
Q_DECLARE_METATYPE(nx::vms::api::DetailedLicenseData)
Q_DECLARE_METATYPE(nx::vms::api::DetailedLicenseDataList)
