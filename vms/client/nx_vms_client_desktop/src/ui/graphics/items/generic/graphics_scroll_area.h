#pragma once

#include <QtWidgets/QGraphicsWidget>

#include <ui/animation/animated.h>

class QnGraphicsScrollAreaPrivate;
class QnGraphicsScrollArea: public Animated<QGraphicsWidget>
{
    Q_OBJECT
    using base_type = Animated<QGraphicsWidget>;

public:
    QnGraphicsScrollArea(QGraphicsItem* parent = nullptr);
    virtual ~QnGraphicsScrollArea();

    QGraphicsWidget* contentWidget() const;
    void setContentWidget(QGraphicsWidget* widget);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    int lineHeight() const;
    void setLineHeight(int value);

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override;

private:
    Q_DECLARE_PRIVATE(QnGraphicsScrollArea)
    const QScopedPointer<QnGraphicsScrollAreaPrivate> d_ptr;
};
