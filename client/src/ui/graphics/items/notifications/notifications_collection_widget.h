#ifndef NOTIFICATIONS_COLLECTION_WIDGET_H
#define NOTIFICATIONS_COLLECTION_WIDGET_H

#include <QtWidgets/QGraphicsItem>

#include <utils/common/connective.h>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/resource_fwd.h>

#include <health/system_health.h>

#include <ui/actions/action_parameters.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;
class QnNotificationWidget;
class QnParticleItem;
class QnToolTipWidget;

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
    void showBalloon();
    void hideBalloon();

    void updateParticleGeometry();
    void updateParticleVisibility();
    void updateToolTip();
    void updateBalloonTailPos();
    void updateBalloonGeometry();

    void at_particle_visibleChanged();

private:
    QnToolTipWidget *m_balloon;
    QnParticleItem *m_particle;
    qint64 m_time;
    int m_count;
};


class QnNotificationsCollectionWidget: public Connective<GraphicsWidget>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<GraphicsWidget> base_type;

public:
    explicit QnNotificationsCollectionWidget(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0, QnWorkbenchContext* context = NULL);
    virtual ~QnNotificationsCollectionWidget();

    /** Geometry of the header widget. */
    QRectF headerGeometry() const;

    /** Combined geometry of all visible sub-wigdets. */
    QRectF visibleGeometry() const;

    /** Rectangle where all tooltips should fit - in local coordinates. */
    void setToolTipsEnclosingRect(const QRectF &rect);

    QnBlinkingImageButtonWidget *blinker() const {
        return m_blinker.data();
    }
    void setBlinker(QnBlinkingImageButtonWidget *blinker);

    Qn::NotificationLevel notificationLevel() const;

signals:
    void visibleSizeChanged();
    void sizeHintChanged();
    void notificationLevelChanged();

private:
    void showSystemHealthMessage(QnSystemHealth::MessageType message, const QVariant& params);
    void hideSystemHealthMessage(QnSystemHealth::MessageType message, const QVariant& params);
    void showBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void hideBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void hideAll();
    void updateBlinker();

    void at_debugButton_clicked();
    void at_list_itemRemoved(QnNotificationWidget *item);
    void at_item_actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters);
    void at_notificationCache_fileDownloaded(const QString& filename, bool ok);

private:
    /**
     * @brief loadThumbnailForItem          Start async thumbnail loading
     * @param item                          Item that will receive loaded thumbnail
     * @param resource                      Camera resource - thumbnail provider
     * @param usecSinceEpoch                Timestamp for the thumbnail, -1 means latest available
     */
    void loadThumbnailForItem(QnNotificationWidget *item, QnResourcePtr resource, qint64 usecsSinceEpoch = -1);

    QnNotificationWidget* findItem(QnSystemHealth::MessageType message, const QnResourcePtr &resource, bool useResource = true);
    QnNotificationWidget* findItem(const QnUuid& businessRuleId, const QnResourcePtr &resource, bool useResource = true);

private:
    QnNotificationWidget* findItem(int businessRuleId, const QnResourcePtr &resource);
    QnNotificationListWidget *m_list;
    GraphicsWidget* m_headerWidget;

    QMultiHash<QnSystemHealth::MessageType, QnNotificationWidget*> m_itemsByMessageType;
    QMultiHash<QString, QnNotificationWidget*> m_itemsByLoadingSound;
    QMultiHash<QnUuid, QnNotificationWidget*> m_itemsByBusinessRuleId;
    QPointer<QnBlinkingImageButtonWidget> m_blinker;
};

#endif // NOTIFICATIONS_COLLECTION_WIDGET_H
