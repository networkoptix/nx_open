#ifndef NOTIFICATIONS_COLLECTION_WIDGET_H
#define NOTIFICATIONS_COLLECTION_WIDGET_H

#include <QtGui/QGraphicsItem>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <core/resource/resource_fwd.h>
#include <health/system_health.h>
#include <ui/actions/action_parameters.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;
class QnNotificationItem;
class QnParticleItem;

/**
 * An image button widget that displays thumbnail behind the button.
 */
class QnBlinkingImageButtonWidget: public QnImageButtonWidget, public AnimationTimerListener {
    Q_OBJECT

    typedef QnImageButtonWidget base_type;

public:
    QnBlinkingImageButtonWidget(QGraphicsItem *parent = NULL);

public slots:
    void setNotificationCount(int count);
    void setColor(const QColor &color);

protected:
    virtual void tick(int deltaMSecs) override;

private slots:
    void updateParticleGeometry();
    void updateParticleVisibility();
    void updateToolTip();

private:
    QnParticleItem *m_particle;
    qint64 m_time;
    int m_count;
};


class QnNotificationsCollectionWidget: public GraphicsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef GraphicsWidget base_type;
public:
    explicit QnNotificationsCollectionWidget(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0, QnWorkbenchContext* context = NULL);
    virtual ~QnNotificationsCollectionWidget();

    /** Geometry of the header widget. */
    QRectF headerGeometry() const;

    /** Combined geometry of all visible sub-wigdets. */
    QRectF visibleGeometry() const;

    /** Rectangle where all tooltips should fit - in local coordinates. */
    void setToolTipsEnclosingRect(const QRectF &rect);

    // TODO: #GDM bad practice.
    // 1. There is not getter, only a setter.
    // 2. QnBlinkingImageButtonWidget is notification-specific (Sets tooltip to 'You have %n notifications') =>
    //      it should be named as such.
    // 3. Locally stored blinker is not owned by this widget, pointer is not protected with QWeakPointer =>
    //      its lifetime is expected to be correctly externally managed. Not something you can rely on in a big project.
    // 4. And why add it as a member? This widget should know nothing about
    //      some button. Just exposing the signals would be enough.
    void setBlinker(QnBlinkingImageButtonWidget* blinker);

signals:
    void visibleSizeChanged();
    void sizeHintChanged();

private slots:
    void showSystemHealthMessage(QnSystemHealth::MessageType message, const QnResourcePtr &resource);
    void hideSystemHealthMessage(QnSystemHealth::MessageType message, const QnResourcePtr &resource);
    void showBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void hideAll();

    void at_settingsButton_clicked();
    void at_filterButton_clicked();
    void at_eventLogButton_clicked();
    void at_debugButton_clicked();
    void at_list_itemRemoved(QnNotificationItem* item);
    void at_item_actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters);

private:
    /**
     * @brief loadThumbnailForItem          Start async thumbnail loading
     * @param item                          Item that will receive loaded thumbnail
     * @param resource                      Camera resource - thumbnail provider
     * @param usecSinceEpoch                Timestamp for the thumbnail, -1 means latest available
     */
    void loadThumbnailForItem(QnNotificationItem *item, QnResourcePtr resource, qint64 usecsSinceEpoch = -1);

    QnNotificationItem* findItem(QnSystemHealth::MessageType message, const QnResourcePtr &resource);

    QnNotificationListWidget *m_list;
    GraphicsWidget* m_headerWidget;

    QMultiHash<QnSystemHealth::MessageType, QnNotificationItem*> m_itemsByMessageType;
    QnBlinkingImageButtonWidget* m_blinker;
};

#endif // NOTIFICATIONS_COLLECTION_WIDGET_H
