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

    QSizeF visibleSize() const;

    void setToolTipsEnclosingRect(const QRectF &rect);
signals:
    void visibleSizeChanged();
    void sizeHintChanged();
    void itemRemoved(QnNotificationItem *item);

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    virtual void tick(int deltaMSecs) override;
    virtual void updateGeometry() override;

    void updateVisibleSize();
private slots:
    void at_item_clicked(Qt::MouseButton button);
    void at_item_geometryChanged();
    void at_geometry_changed();

private:
    struct ItemData {
        enum State {
            Collapsing,
            Collapsed,
            Displaying,
            Displayed,
            Hiding,
            Hidden
        };

        bool isVisible() const {
            return state == Displaying || state == Displayed || state == Hiding || state == Collapsing;
        }

        void unlockAndHide() {
            locked = false;
            hide();
        }

        void hide();

        void setAnimation(qreal from, qreal to, qreal time);
        void animationTick(qreal deltaMSecs);
        bool animationFinished();

        struct {
            qreal value;
            qreal source;
            qreal target;
            qreal length;
        } animation;

        QnNotificationItem* item;
        State state;
        bool locked;
        int cachedHeight;
    };

    HoverFocusProcessor* m_hoverProcessor;

    /**
     * @brief m_items       List of all items. Strictly ordered by item state:
     *                      (Displayed|Hiding|Hidden)* (Displaying)? (Collapsing)* (Collapsed)*
     *                      Item that is closer to the beginning of the list is displayed earlier.
     */
    QLinkedList<QnNotificationItem *> m_items;

    QMap<QnNotificationItem*, ItemData*> m_itemDataByItem;

    ItemData m_collapser;
    bool m_collapsedItemCountChanged;
    QSizeF m_visibleSize;
    QRectF m_tooltipsEnclosingRect;
};

#endif // NOTIFICATION_LIST_WIDGET_H
