#pragma once

#include "data.h"

#include <QtCore/QString>
#include <QtCore/QtGlobal>
#include <QtCore/QByteArray>

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API LockData: Data
{
    QString name;
    QnUuid peer;
    qint64 timestamp = 0;
    QByteArray userData;
};
#define LockData_Fields (name)(peer)(timestamp)(userData)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::LockData)
