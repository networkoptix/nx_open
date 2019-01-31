#pragma once

#include <private/qsgvideonode_p.h>
#include <QtMultimedia/qvideosurfaceformat.h>

class QnBpiSGVideoMaterial_YUV;

class QnBpiSGVideoNode_YUV: public QSGVideoNode
{
public:
    QnBpiSGVideoNode_YUV(const QVideoSurfaceFormat &format);
    ~QnBpiSGVideoNode_YUV();

    virtual QVideoFrame::PixelFormat pixelFormat() const { return m_format.pixelFormat(); }
    QAbstractVideoBuffer::HandleType handleType() const { return QAbstractVideoBuffer::NoHandle; }
    void setCurrentFrame(const QVideoFrame &frame, FrameFlags flags);

private:
    void bindTexture(int id, int unit, int w, int h, const uchar* bits);

    QVideoSurfaceFormat m_format;
    QnBpiSGVideoMaterial_YUV* m_material;
};

class QnBpiSGVideoNodeFactory_YUV: public QSGVideoNodeFactoryInterface
{
public:
    virtual QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType) const override;

    virtual QSGVideoNode* createNode(const QVideoSurfaceFormat& format) override;
};
