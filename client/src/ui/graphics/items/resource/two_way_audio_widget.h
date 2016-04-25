#pragma once

#include <ui/customization/customized.h>
#include <ui/graphics/items/standard/graphics_widget.h>

#include <core/resource/resource_fwd.h>

class QnTwoWayAudioWidgetPrivate;

class QnTwoWayAudioWidget : public Customized<GraphicsWidget>
{
    Q_OBJECT
    typedef Customized<GraphicsWidget> base_type;

public:
    QnTwoWayAudioWidget(QGraphicsWidget *parent = nullptr);
    ~QnTwoWayAudioWidget();

    void setCamera(const QnVirtualCameraResourcePtr &camera);

    void setFixedHeight(qreal height);
protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QScopedPointer<QnTwoWayAudioWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnTwoWayAudioWidget)
};
