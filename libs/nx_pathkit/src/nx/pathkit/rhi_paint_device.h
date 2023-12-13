// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPaintDevice>

namespace nx::pathkit {

class RhiPaintEngine;

/**
 * A paint device for QPainter which records paint commands and can be later rendered with
 * RhiPaintDeviceRenderer.
 */
class NX_PATHKIT_API RhiPaintDevice: public QPaintDevice
{
    using base_type = QPaintDevice;

public:
    RhiPaintDevice();
    ~RhiPaintDevice();

    void setSize(QSize size) { m_size = size; }
    void setDevicePixelRatio(qreal scaleFactor);

    void clear();

    QPaintEngine* paintEngine() const override;

protected:
    int metric(QPaintDevice::PaintDeviceMetric metric) const override;

private:
    QSize m_size;
    qreal m_pixelRatio = 1.0;
    qreal m_dpmx;
    qreal m_dpmy;

    std::unique_ptr<RhiPaintEngine> m_paintEngine;
};

} // namespace nx::pathkit
