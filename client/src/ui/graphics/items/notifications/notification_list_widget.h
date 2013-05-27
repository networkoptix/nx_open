#ifndef NOTIFICATION_LIST_WIDGET_H
#define NOTIFICATION_LIST_WIDGET_H

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnNotificationItem;

class QnNotificationListWidget : public Animated<GraphicsWidget>, public AnimationTimerListener
{
    Q_OBJECT
    typedef Animated<GraphicsWidget> base_type;
public:
    explicit QnNotificationListWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags flags = 0);
    ~QnNotificationListWidget();

    void addItem(QnNotificationItem *item);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const;

    virtual void tick(int deltaMSecs) override;

private slots:
    void at_item_geometryChanged();

private:
    QList<QnNotificationItem*> m_items;
};

#endif // NOTIFICATION_LIST_WIDGET_H
