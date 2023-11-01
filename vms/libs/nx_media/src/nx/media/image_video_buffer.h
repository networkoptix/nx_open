// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtMultimedia/private/qabstractvideobuffer_p.h>

#include <nx/media/media_fwd.h>

namespace nx::media {

/**
 * This class implements Qt video buffer interface using underlying QImage data.
 */
class NX_MEDIA_API ImageVideoBuffer: public QAbstractVideoBuffer
{
public:
    ImageVideoBuffer(const QImage& image):
        QAbstractVideoBuffer(QVideoFrame::NoHandle),
        m_image(image)
    {
    }

    virtual ~ImageVideoBuffer()
    {
    }

    virtual QVideoFrame::MapMode mapMode() const override
    {
        return m_mapMode;
    }

    virtual MapData map(QVideoFrame::MapMode mode) override;

    virtual void unmap() override;

private:
    const QImage m_image;
    QVideoFrame::MapMode m_mapMode = QVideoFrame::NotMapped;
};

VideoFramePtr videoFrameFromImage(const QImage& image);

} // namespace nx::media
