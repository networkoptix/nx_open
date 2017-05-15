#pragma once

#include <ui/animation/animated.h>
#include <ui/graphics/items/overlays/overlayed.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>

class QnViewportBoundWidget;
class RectAnimator;
class AnimationTimer;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class LayoutTourDropPlaceholder: public Overlayed<Animated<QnFramedWidget>>
{
    Q_OBJECT
    using base_type = Overlayed<Animated<QnFramedWidget>>;
public:
    LayoutTourDropPlaceholder(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = 0);

    virtual QRectF boundingRect() const override;

    const QRectF& rect() const;
    void setRect(const QRectF& rect);

    AnimationTimer* animationTimer() const;
    void setAnimationTimer(AnimationTimer* timer);

protected:
    virtual void changeEvent(QEvent* event) override;

private:
    QRectF m_rect;
    QnViewportBoundWidget* m_widget;
    RectAnimator* m_geometryAnimator;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
