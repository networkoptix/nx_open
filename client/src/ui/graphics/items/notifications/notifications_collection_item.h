#ifndef NOTIFICATIONS_COLLECTION_ITEM_H
#define NOTIFICATIONS_COLLECTION_ITEM_H

#include <QtGui/QGraphicsItem>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <core/resource/resource_fwd.h>
#include <health/system_health.h>
#include <ui/actions/action_parameters.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;
class QnNotificationItem;

class QnNotificationsCollectionItem : public GraphicsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef GraphicsWidget base_type;
public:
    explicit QnNotificationsCollectionItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0, QnWorkbenchContext* context = NULL);
    virtual ~QnNotificationsCollectionItem();

    /** Geometry of the header widget. */
    QRectF headerGeometry() const;

    /** Combined geometry of all visible sub-wigdets. */
    QRectF visibleGeometry() const;

signals:
    void visibleSizeChanged();

public slots:
    void showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr &resource);
    void showBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void hideAll();

private slots:
    void at_settingsButton_clicked();
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

    QnNotificationListWidget *m_list;
    QGraphicsWidget* m_headerWidget;
};

#endif // NOTIFICATIONS_COLLECTION_ITEM_H
