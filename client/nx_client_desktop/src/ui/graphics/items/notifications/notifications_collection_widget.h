#ifndef NOTIFICATIONS_COLLECTION_WIDGET_H
#define NOTIFICATIONS_COLLECTION_WIDGET_H

#include <QtWidgets/QGraphicsItem>

#include <utils/common/connective.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>

#include <core/resource/resource_fwd.h>

#include <health/system_health.h>

#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/client/desktop/ui/actions/actions.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;
class QnNotificationWidget;
class QnParticleItem;
class QnToolTipWidget;
class QnBlinkingImageButtonWidget;

namespace nx { namespace vms { namespace event { class StringsHelper; }}}

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

    void addAcknoledgeButtonIfNeeded(QnNotificationWidget* widget,
        const QnVirtualCameraResourcePtr& camera,
        const nx::vms::event::AbstractActionPtr& action);

    using ParametersGetter = std::function<nx::client::desktop::ui::action::Parameters ()>;
    QnNotificationWidget* addCustomPopup(
        nx::client::desktop::ui::action::IDType actionId,
        const ParametersGetter& parametersGetter,
        QnNotificationLevel::Value notificationLevel,
        const QString& buttonText,
        bool closeable);
    void showEventAction(const nx::vms::event::AbstractActionPtr& businessAction);
    void hideEventAction(const nx::vms::event::AbstractActionPtr& businessAction);

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
    QnNotificationWidget* findItem(const QnUuid& eventRuleId, const QnResourcePtr &resource);
    QnNotificationWidget* findItem(const QnUuid& eventRuleId, std::function<bool(QnNotificationWidget* item)> filter);

    QIcon iconForAction(const nx::vms::event::AbstractActionPtr& action) const;

    void cleanUpItem(QnNotificationWidget* item);

private:
    QnNotificationListWidget *m_list;
    GraphicsWidget* m_headerWidget;

    QMultiHash<QnSystemHealth::MessageType, QnNotificationWidget*> m_itemsByMessageType;
    QMultiHash<QString, QnNotificationWidget*> m_itemsByLoadingSound;
    QMultiHash<QnUuid, QnNotificationWidget*> m_itemsByEventRuleId;
    QPointer<QnBlinkingImageButtonWidget> m_blinker;
    std::unique_ptr<nx::vms::event::StringsHelper> m_helper;
    QHash<QnUuid, QnNotificationWidget*> m_customPopupItems;
    QnNotificationWidget* m_currentDefaultPasswordChangeWidget = nullptr;
};

#endif // NOTIFICATIONS_COLLECTION_WIDGET_H
