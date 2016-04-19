#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/graphics/items/generic/image_button_widget.h>

class QnTwoWayAudioWidgetPrivate;

class QnTwoWayAudioWidget : public QnImageButtonWidget
{
    Q_OBJECT
    typedef QnImageButtonWidget base_type;

public:
    QnTwoWayAudioWidget(QGraphicsWidget *parent = nullptr);
    ~QnTwoWayAudioWidget();

    void setCamera(const QnVirtualCameraResourcePtr &camera);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual void pressedNotify(QGraphicsSceneMouseEvent *event) override;
    virtual void releasedNotify(QGraphicsSceneMouseEvent *event) override;

private:
    QScopedPointer<QnTwoWayAudioWidgetPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnTwoWayAudioWidget)
};
