// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/common/utils/geometry.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API Geometry: public common::Geometry
{
    Q_OBJECT

public:
    static void registerQmlType();
};

} // namespace nx::vms::client::core
