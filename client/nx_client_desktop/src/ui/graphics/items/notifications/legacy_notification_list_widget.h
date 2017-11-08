#pragma once

#include <QtCore/QLinkedList>

#include <client/client_globals.h>

#include <ui/animation/animated.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/standard/graphics_widget.h>

class QnNotificationWidget;
class HoverFocusProcessor;

class QnNotificationListWidget : public Animated<GraphicsWidget>, public AnimationTimerListener
{
    Q_OBJECT
    typedef Animated<GraphicsWidget> base_type;

public:
    explicit QnNotificationListWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationListWidget();

    void addItem(QnNotificationWidget *item, bool locked = false);
    void removeItem(QnNotificationWidget *item);
    void clear();

    QSizeF visibleSize() const;

    /** Rectangle where all tooltips should fit - in local coordinates. */
    void setToolTipsEnclosingRect(const QRectF &rect);

    int itemCount() const;
    QnNotificationLevel::Value notificationLevel() const;

signals:
    void visibleSizeChanged();
    void sizeHintChanged();

    void itemAdded(QnNotificationWidget *item);
    void itemRemoved(QnNotificationWidget *item);
    void itemCountChanged();
    void notificationLevelChanged();

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    virtual void tick(int deltaMSecs) override;
    virtual void updateGeometry() override;

    void updateVisibleSize();

private slots:
    void at_item_closeTriggered();
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

        void unlockAndHide(qreal speedUp) {
            locked = false;
            hide(speedUp);
        }

        void hide(qreal speedUp);

        void setAnimation(qreal from, qreal to, qreal time);
        void animationTick(qreal deltaMSecs);
        bool animationFinished();

        struct {
            qreal value;
            qreal source;
            qreal target;
            qreal length;
        } animation;

        QnNotificationWidget* item;
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
    QLinkedList<QnNotificationWidget *> m_items;

    QMap<QnNotificationWidget*, ItemData*> m_itemDataByItem;

    ItemData m_collapser;
    bool m_collapsedItemCountChanged;
    qreal m_speedUp;
    QSizeF m_visibleSize;
    QRectF m_tooltipsEnclosingRect;
    QnNotificationLevel::Value m_itemNotificationLevel;
};
