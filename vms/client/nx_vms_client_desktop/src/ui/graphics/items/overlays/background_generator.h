// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QImage>

namespace nx::vms::client::desktop {
QImage generatePlaceholderBackground(int width,
    int height,
    const QRect& rect,
    int maskSize,
    const QColor& foreground,
    const QColor& background);
};
