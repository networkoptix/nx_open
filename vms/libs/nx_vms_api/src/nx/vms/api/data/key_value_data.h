#pragma once

#include "data.h"

#include <QtCore/QByteArray>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API KeyValueData: Data
{
    KeyValueData() = default;
    KeyValueData(const QByteArray& name, const QByteArray& value): name(name), value(value) {}

    QByteArray name;
    QByteArray value;
};

#define KeyValueData_Fields \
    (name)(value)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::KeyValueData)
Q_DECLARE_METATYPE(nx::vms::api::KeyValueDataList)
