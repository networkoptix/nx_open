#pragma once

#include <ui/graphics/items/resource/web_resource_widget.h>

namespace nx {
namespace client {
namespace desktop {

class C2pResourceWidget: public QnWebResourceWidget
{
    Q_OBJECT

    using base_type = QnWebResourceWidget;
public:
    C2pResourceWidget(
        QnWorkbenchContext* context,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);

    Q_INVOKABLE void c2pplayback(const QString& cameraNames, int timestamp);

private:
    void resetC2pLayout(const QnVirtualCameraResourceList& cameras, qint64 timestampMs);
};


} // namespace desktop
} // namespace client
} // namespace nx
