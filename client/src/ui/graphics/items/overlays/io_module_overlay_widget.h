#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <client/client_color_types.h>

class QnIoModuleOverlayWidgetPrivate;
class QnIoModuleOverlayWidget : public GraphicsWidget {
    Q_OBJECT
    typedef GraphicsWidget base_type;
    Q_PROPERTY(QnIoModuleColors colors READ colors WRITE setColors)

public:
    QnIoModuleOverlayWidget(QGraphicsWidget *parent = nullptr);
    ~QnIoModuleOverlayWidget();

    void setCamera(const QnVirtualCameraResourcePtr &camera);

    const QnIoModuleColors &colors() const;
    void setColors(const QnIoModuleColors &colors);

private:
    QScopedPointer<QnIoModuleOverlayWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnIoModuleOverlayWidget)
};
