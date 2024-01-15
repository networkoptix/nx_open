// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QInternal>

#include <nx/reflect/instrument.h>

namespace nx::core::transcoding {

struct FilterParams
{
    bool enabled = false;
    Qt::Corner corner = Qt::BottomRightCorner;
    bool operator==(const FilterParams&) const = default;
};
NX_REFLECTION_INSTRUMENT(FilterParams, (enabled)(corner))

} // namespace nx::core::transcoding
