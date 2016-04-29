#include "bpi_video_node_plugin.h"
#include "bpi_video_node.h"

namespace
{
    //const QVideoFrame::PixelFormat Format_Tiled32x32NV12 = QVideoFrame::Format_NV12;
    const QVideoFrame::PixelFormat Format_Tiled32x32NV12 = QVideoFrame::PixelFormat(QVideoFrame::Format_User + 17);
}


QList<QVideoFrame::PixelFormat> QnBpiSGVideoNodeFactoryPlugin::supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType) const
{
    QList<QVideoFrame::PixelFormat> pixelFormats;

    if (handleType == QAbstractVideoBuffer::NoHandle)
        pixelFormats.append(Format_Tiled32x32NV12);

    return pixelFormats;
}

QSGVideoNode *QnBpiSGVideoNodeFactoryPlugin::createNode(const QVideoSurfaceFormat &format)
{
    if (supportedPixelFormats(format.handleType()).contains(format.pixelFormat()))
        return new QSGVideoNode_YUV(format);

    return 0;
}
