#ifndef NOTIFICATIONS_COLLECTION_WIDGET_H
#define NOTIFICATIONS_COLLECTION_WIDGET_H

#include <QtWidgets/QGraphicsItem>

#include <utils/common/connective.h>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>

#include <core/resource/resource_fwd.h>

#include <health/system_health.h>

#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;
class QnNotificationWidget;
class QnParticleItem;
class QnToolTipWidget;
class QnBlinkingImageButtonWidget;
class QnBusinessStringsHelper;

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

    QnBlinkingImageButtonWidget* blinker() const;
    void setBlinker(QnBlinkingImageButtonWidget* blinker);

    QnNotificationLevel::Value notificationLevel() const;

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget = nullptr) override;

signals:
    void visibleSizeChanged();
    void sizeHintChanged();
    void notificationLevelChanged();

private:
    void showSystemHealthMessage(QnSystemHealth::MessageType message, const QVariant& params);
    void hideSystemHealthMessage(QnSystemHealth::MessageType message, const QVariant& params);
    void showBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void handleShowPopupAction(
        const QnAbstractBusinessActionPtr& businessAction,
        QnNotificationWidget* widget);

    void hideBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void hideAll();
    void updateBlinker();

    void at_list_itemRemoved(QnNotificationWidget *item);
    void at_item_actionTriggered(nx::client::desktop::ui::action::IDType actionId,
        const nx::client::desktop::ui::action::Parameters& parameters);
    void at_notificationCache_fileDownloaded(const QString& filename);

private:
    /**
     * @brief loadThumbnailForItem          Start async thumbnail loading
     * @param item                          Item that will receive loaded thumbnail
     * @param resource                      Camera resource - thumbnail provider
     * @param msecSinceEpoch                Timestamp for the thumbnail, -1 means latest available
     */
    void loadThumbnailForItem(QnNotificationWidget *item, const QnVirtualCameraResourcePtr &camera, qint64 msecSinceEpoch = -1);
    void loadThumbnailForItem(QnNotificationWidget *item, const QnVirtualCameraResourceList &cameraList, qint64 msecSinceEpoch = -1);

    QnNotificationWidget* findItem(QnSystemHealth::MessageType message, const QnResourcePtr &resource);
    QnNotificationWidget* findItem(const QnUuid& businessRuleId, const QnResourcePtr &resource);
    QnNotificationWidget* findItem(const QnUuid& businessRuleId, std::function<bool(QnNotificationWidget* item)> filter);

    QIcon iconForAction(const QnAbstractBusinessActionPtr& businessAction) const;

    void cleanUpItem(QnNotificationWidget* item);
private:
    QnNotificationListWidget *m_list;
    GraphicsWidget* m_headerWidget;

    QMultiHash<QnSystemHealth::MessageType, QnNotificationWidget*> m_itemsByMessageType;
    QMultiHash<QString, QnNotificationWidget*> m_itemsByLoadingSound;
    QMultiHash<QnUuid, QnNotificationWidget*> m_itemsByBusinessRuleId;
    QPointer<QnBlinkingImageButtonWidget> m_blinker;
    std::unique_ptr<QnBusinessStringsHelper> m_helper;
};

#endif // NOTIFICATIONS_COLLECTION_WIDGET_H
