// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "blur_filter.h"

class QImage;

/** QWidgets-free Drop Shadow Filter. If you can use QWidgets, use QPixmapDropShadowFilter instead. */

namespace nx::utils::graphics
{

class DropShadowFilter
{
public:
    DropShadowFilter(int dx = 0 , int dy = 2, int blurWidth = 8);

    /**  Blurs image. Image edges (of blurWidth) are not processed. */
    void filterImage(QImage& image);

private:
    BlurFilter m_blurFilter;

    int m_dx, m_dy;
};

} // namespace nx::utils::graphics
