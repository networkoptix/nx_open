// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API UpdateUploadData
{
    QString updateId;
    QByteArray data;
    qint64 offset = 0;
};
#define UpdateUploadData_Fields \
    (updateId) \
    (data) \
    (offset)
NX_VMS_API_DECLARE_STRUCT(UpdateUploadData)

struct NX_VMS_API UpdateUploadResponseData
{
    QnUuid id;
    QString updateId;
    int chunks = 0;
};
#define UpdateUploadResponseData_Fields (id)(updateId)(chunks)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(UpdateUploadResponseData)

struct NX_VMS_API UpdateInstallData
{
    QString updateId;
};
#define UpdateInstallData_Fields (updateId)
NX_VMS_API_DECLARE_STRUCT(UpdateInstallData)

} // namespace api
} // namespace vms
} // namespace nx
