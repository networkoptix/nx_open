#include "bpi_video_node_plugin.h"
#include "bpi_video_node.h"

QList<QVideoFrame::PixelFormat> QnBpiSGVideoNodeFactoryPlugin::supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> pixelFormats;

    if (handleType == QAbstractVideoBuffer::GLTextureHandle)
        pixelFormats.append(QVideoFrame::Format_BGR32);

    return pixelFormats;
}

QSGVideoNode *QnBpiSGVideoNodeFactoryPlugin::createNode(const QVideoSurfaceFormat &format)
{
    if (supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return new QnBpiSGVideoNode(format);

    return 0;
}
