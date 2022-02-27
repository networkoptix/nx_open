// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API LockData
{
    QString name;
    QnUuid peer;
    qint64 timestamp = 0;
    QByteArray userData;
};
#define LockData_Fields (name)(peer)(timestamp)(userData)
NX_VMS_API_DECLARE_STRUCT(LockData)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::LockData)
