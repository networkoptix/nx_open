#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnIoModuleOverlayWidgetPrivate;
class QnIoModuleOverlayWidget : public GraphicsWidget {
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    QnIoModuleOverlayWidget(QGraphicsWidget *parent = nullptr);
    ~QnIoModuleOverlayWidget();

    void setCamera(const QnVirtualCameraResourcePtr &camera);

private:
    QnIoModuleOverlayWidgetPrivate *d;
};
