// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_snaps_geometry.h"
#include "screen_utils.h"

QRect screenSnapsToWidgetGeometry(const QnScreenSnaps& snaps, QWidget* widget)
{
    Q_UNUSED(widget);

    return screenSnapsGeometry(snaps, nx::gui::Screens::logicalGeometries());
}
