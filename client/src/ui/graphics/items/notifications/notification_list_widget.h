#ifndef NOTIFICATION_LIST_WIDGET_H
#define NOTIFICATION_LIST_WIDGET_H

#include <QtCore/QLinkedList>

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnNotificationItem;
class HoverFocusProcessor;

class QnItemState: public QObject {
    Q_OBJECT
public:
    explicit QnItemState(QObject *parent = NULL):
        QObject(parent){}

    Q_SLOT void unlockAndHide(Qt::MouseButton button) {
        if (button != Qt::RightButton)
            return;

        locked = false;
        hide();
    }

    enum State {
        Waiting,
        Displaying,
        Displayed,
        Hiding,
        Hidden
    };

    bool isVisible() const {
        return state == Displaying || state == Displayed || state == Hiding;
    }

    void hide() {
        state = QnItemState::Hiding;
        targetValue = 0.0;
    }

    QnNotificationItem* item;
    State state;
    qreal targetValue;
    bool locked;

};

class QnNotificationListWidget : public Animated<GraphicsWidget>, public AnimationTimerListener
{
    Q_OBJECT
    typedef Animated<GraphicsWidget> base_type;
public:
    explicit QnNotificationListWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags flags = 0);
    ~QnNotificationListWidget();

    void addItem(QnNotificationItem *item, bool locked = false);

signals:
    void itemRemoved(QnNotificationItem *item);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const;

    virtual void tick(int deltaMSecs) override;
private slots:
    void at_item_geometryChanged();

private:
    HoverFocusProcessor* m_hoverProcessor;
    QLinkedList<QnItemState *> m_items;
    int m_counter;
};

#endif // NOTIFICATION_LIST_WIDGET_H
