// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core {

NX_REFLECTION_ENUM_CLASS(StandardRotation,
    rotate0 = 0,
    rotate90 = 90,
    rotate180 = 180,
    rotate270 = 270
);

class NX_VMS_CLIENT_CORE_API Rotation
{
public:
    Rotation() = default;
    explicit Rotation(qreal degrees);
    explicit Rotation(StandardRotation standardRotation);

    qreal value() const;
    QString toString() const;

    using RotationList = QList<StandardRotation>;
    static RotationList standardRotations();
    static Rotation closestStandardRotation(qreal degrees);
    static StandardRotation standardRotation(qreal degrees);

    bool operator==(const Rotation& other) const;
    bool operator!=(const Rotation& other) const;

private:
    qreal m_degrees = 0;
};

} // namespace nx::vms::client::core
