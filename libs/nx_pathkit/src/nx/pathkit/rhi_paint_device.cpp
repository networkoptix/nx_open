// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rhi_paint_device.h"
#include "rhi_paint_engine.h"

// These functions are exported from Qt.
int qt_defaultDpiX();
int qt_defaultDpiY();

namespace {

static constexpr qreal kCmPerInch = 2.54;

} // namespace

namespace nx::pathkit {

RhiPaintDevice::RhiPaintDevice():
    m_dpmx(qt_defaultDpiX() * 100 / kCmPerInch), //< From QImage sources.
    m_dpmy(qt_defaultDpiY() * 100 / kCmPerInch)
{
    m_paintEngine = std::make_unique<RhiPaintEngine>(this);
}

RhiPaintDevice::~RhiPaintDevice()
{}

QPaintEngine* RhiPaintDevice::paintEngine() const
{
    return m_paintEngine.get();
}

int RhiPaintDevice::metric(QPaintDevice::PaintDeviceMetric metric) const
{
    switch (metric)
    {
        case PdmWidth:
            return m_size.width();
        case PdmHeight:
            return m_size.height();

        case PdmWidthMM:
            return qRound(m_size.width() * 1000 / m_dpmx);
        case PdmHeightMM:
            return qRound(m_size.height() * 1000 / m_dpmy);

        case PdmDpiX:
            return qRound(m_dpmx * kCmPerInch / 100.0);
        case PdmDpiY:
            return qRound(m_dpmy * kCmPerInch / 100.0);

        case PdmPhysicalDpiX:
            return qRound(m_dpmx * kCmPerInch / 100.0);
        case PdmPhysicalDpiY:
            return qRound(m_dpmy * kCmPerInch / 100.0);

        case PdmDevicePixelRatio:
            return m_pixelRatio;
        case PdmDevicePixelRatioScaled:
            return m_pixelRatio * QPaintDevice::devicePixelRatioFScale();

        default:
            return base_type::metric(metric);
    }
}

void RhiPaintDevice::setDevicePixelRatio(qreal scaleFactor)
{
    m_pixelRatio = scaleFactor;
}

void RhiPaintDevice::clear()
{
    m_paintEngine->m_paths.clear();
}

} // namespace nx::pathkit
