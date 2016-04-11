#pragma once

#include <private/qsgvideonode_p.h>
#include <qmutex.h>

class QnBpiSGVideoNodeMaterial;

class QnBpiSGVideoNode : public QSGVideoNode
{
public:
    QnBpiSGVideoNode(const QVideoSurfaceFormat &format);
    ~QnBpiSGVideoNode();

    void setCurrentFrame(const QVideoFrame &frame, FrameFlags flags);
    QVideoFrame::PixelFormat pixelFormat() const { return m_format.pixelFormat(); }
    QAbstractVideoBuffer::HandleType handleType() const { return QAbstractVideoBuffer::GLTextureHandle; }

    void preprocess();

private:
    QBpiSGVideoNodeMaterial *m_material;
    QMutex m_frameMutex;
    QVideoFrame m_frame;
    QVideoSurfaceFormat m_format;
};
