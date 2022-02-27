// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QMetaType>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API KeyValueData
{
    KeyValueData() = default;
    KeyValueData(const QByteArray& name, const QByteArray& value): name(name), value(value) {}

    QByteArray name;
    QByteArray value;
};

#define KeyValueData_Fields \
    (name)(value)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(KeyValueData)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::KeyValueData)
Q_DECLARE_METATYPE(nx::vms::api::KeyValueDataList)
