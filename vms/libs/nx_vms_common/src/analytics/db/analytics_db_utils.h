// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

#include "config.h"

namespace nx::analytics::db {

static constexpr QRect kTrackSearchGrid =
    QRect(0, 0, kTrackSearchResolutionX, kTrackSearchResolutionY);

NX_VMS_COMMON_API QRect translateToSearchGrid(const QRectF& box);

bool rectsIntersectToSearchPrecision(const QRectF& one, const QRectF& two);

} // namespace nx::analytics::db
