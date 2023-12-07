// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDateTime>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API DateRange
{
    Q_GADGET
    Q_PROPERTY(QDateTime start MEMBER start)
    Q_PROPERTY(QDateTime end MEMBER end)

public:
    QDateTime start;
    QDateTime end;

    bool operator==(const DateRange&) const = default;
};

} // namespace nx::vms::client::core
