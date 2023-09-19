// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>

namespace Qn {

NX_REFLECTION_ENUM(PtzObjectType,
    PresetPtzObject,
    TourPtzObject,

    InvalidPtzObject = -1
)

} // namespace Qn

struct QnPtzObject
{
    QnPtzObject(): type(Qn::InvalidPtzObject) {}
    QnPtzObject(Qn::PtzObjectType type, const QString &id): type(type), id(id) {}

    Qn::PtzObjectType type;
    QString id;

    bool operator==(const QnPtzObject& other) const = default;
};
#define QnPtzObject_Fields (type)(id)
