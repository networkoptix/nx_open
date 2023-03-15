// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>

#include <nx/reflect/enum_instrument.h>

class QJsonValue;

namespace nx::vms::api::rules {

NX_REFLECTION_ENUM_CLASS(State,
    none,
    started,
    stopped,
    instant
)

using PropertyMap = QMap<QString, QJsonValue>;

} // namespace nx::vms::api::rules
