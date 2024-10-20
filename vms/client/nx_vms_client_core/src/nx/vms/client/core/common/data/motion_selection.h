// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVector>
#include <QtGui/QRegion>

#include <nx/vms/common/serialization/qt_gui_types.h>

namespace nx::vms::client::core {

/** One region per channel. */
using MotionSelection = QVector<QRegion>;

} // namespace nx::vms::client::core
