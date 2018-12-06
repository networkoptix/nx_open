#pragma once

#include "id_data.h"

#include <QtCore/QtGlobal>
#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API UpdateUploadData: Data
{
    QString updateId;
    QByteArray data;
    qint64 offset = 0;
};
#define UpdateUploadData_Fields \
    (updateId) \
    (data) \
    (offset)

struct NX_VMS_API UpdateUploadResponseData: IdData
{
public:
    QString updateId;
    int chunks = 0;
};
#define UpdateUploadResponseData_Fields \
    IdData_Fields \
    (updateId) \
    (chunks)

struct NX_VMS_API UpdateInstallData: Data
{
    QString updateId;
};
#define UpdateInstallData_Fields (updateId)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::UpdateUploadData)
Q_DECLARE_METATYPE(nx::vms::api::UpdateUploadResponseData)
Q_DECLARE_METATYPE(nx::vms::api::UpdateUploadResponseDataList)
Q_DECLARE_METATYPE(nx::vms::api::UpdateInstallData)
