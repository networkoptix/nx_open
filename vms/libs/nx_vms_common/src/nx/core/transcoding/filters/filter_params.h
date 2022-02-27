// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/qnamespace.h>

namespace nx::core::transcoding {

struct FilterParams
{
    bool enabled = false;
    Qt::Corner corner = Qt::BottomRightCorner;
};

} // namespace nx::core::transcoding
