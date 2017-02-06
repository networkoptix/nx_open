#pragma once

#include <private/qsgvideonode_p.h>

class QnBpiSGVideoNodeFactoryPlugin : public QSGVideoNodeFactoryPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QSGVideoNodeFactoryInterface_iid
                      FILE "bpi_videonode.json")

public:
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const;
    QSGVideoNode *createNode(const QVideoSurfaceFormat &format);
};
