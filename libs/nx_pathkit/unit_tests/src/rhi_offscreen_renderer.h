// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtGui/QPaintEngine>

#include <nx/pathkit/rhi_paint_engine.h>
#include <nx/pathkit/rhi_render.h>

class QRhi;
class QRhiRenderPassDescriptor;

namespace nx::pathkit {

class NX_PATHKIT_API RhiOffscreenRenderer
{
public:
    RhiOffscreenRenderer(QRhi* rhi, QSize size, QColor clearColor);
    ~RhiOffscreenRenderer();

    QImage render(RhiPaintDeviceRenderer* renderer);

private:
    QSize m_size;
    QRhi* const m_rhi;
    QColor m_clearColor;

    std::unique_ptr<QRhiRenderPassDescriptor> rp;
};

} // namespace nx::pathkit
