// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "resource_types.h"

#include <QtCore/QMetaType>

namespace nx::vms::api {

Q_NAMESPACE_EXPORT(NX_VMS_API)
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

Q_ENUM_NS(ResourceStatus)

inline size_t qHash(ResourceStatus value, size_t seed = 0)
{
    return QT_PREPEND_NAMESPACE(qHash)((int) value, seed);
}

} // namespace nx::vms::api
