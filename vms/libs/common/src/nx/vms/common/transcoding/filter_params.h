#pragma once

#include <QtCore/qnamespace.h>

namespace nx::vms::common::transcoding {

struct FilterParams
{
    bool enabled = false;
    Qt::Corner corner = Qt::BottomRightCorner;
};

} // namespace nx::vms::common::transcoding
