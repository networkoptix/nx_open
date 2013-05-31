#ifndef NOTIFICATION_LIST_WIDGET_H
#define NOTIFICATION_LIST_WIDGET_H

#include <QtCore/QLinkedList>

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnNotificationItem;
class HoverFocusProcessor;

class QnNotificationListWidget : public Animated<GraphicsWidget>, public AnimationTimerListener
{
    Q_OBJECT
    typedef Animated<GraphicsWidget> base_type;
public:
    explicit QnNotificationListWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags flags = 0);
    ~QnNotificationListWidget();

    void addItem(QnNotificationItem *item, bool locked = false);
    void clear();

signals:
    void itemRemoved(QnNotificationItem *item);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint) const;

    virtual void tick(int deltaMSecs) override;
private slots:
    void at_item_clicked(Qt::MouseButton button);
    void at_geometry_changed();

private:
    struct ItemData {
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

        void unlockAndHide() {
            locked = false;
            hide();
        }

        void hide() {
            state = Hiding;
            targetValue = 0.0;
        }

        QnNotificationItem* item;
        State state;
        qreal targetValue;
        bool locked;
    };

    HoverFocusProcessor* m_hoverProcessor;

    /**
     * @brief m_items       List of all items. Strictly ordered by item state:
     *                      (Displayed|Hiding|Hidden)* (Displaying)? (Waiting)*
     */
    QLinkedList<QnNotificationItem *> m_items;
    QMap<QnNotificationItem*, ItemData*> m_itemDataByItem;
};

#endif // NOTIFICATION_LIST_WIDGET_H
