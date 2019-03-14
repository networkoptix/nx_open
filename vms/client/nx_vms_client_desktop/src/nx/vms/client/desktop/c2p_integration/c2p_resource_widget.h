#pragma once

#include <chrono>

#include <ui/graphics/items/resource/web_resource_widget.h>

namespace nx::vms::client::desktop {

class C2pResourceWidget: public QnWebResourceWidget
{
    Q_OBJECT

    using base_type = QnWebResourceWidget;
public:
    C2pResourceWidget(
        QnWorkbenchContext* context,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);

    Q_INVOKABLE void c2pplayback(const QString& cameraNames, int timestampSec);

private:
    void resetC2pLayout(const QnVirtualCameraResourceList& cameras,
        std::chrono::milliseconds timestamp);
};


} // namespace nx::vms::client::desktop
