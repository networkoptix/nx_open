#ifndef NOTIFICATIONS_COLLECTION_ITEM_H
#define NOTIFICATIONS_COLLECTION_ITEM_H

#include <QtGui/QGraphicsItem>

#include <business/actions/abstract_business_action.h>
#include <business/events/abstract_business_event.h>
#include <core/resource/resource_fwd.h>
#include <health/system_health.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/workbench/workbench_context_aware.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;
class QnNotificationItem;
class QnActionParameters;

class QnNotificationsCollectionItem : public GraphicsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef GraphicsWidget base_type;
public:
    explicit QnNotificationsCollectionItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0, QnWorkbenchContext* context = NULL);
    virtual ~QnNotificationsCollectionItem();

    QRectF headerGeometry() const;

signals:
    void settingsRequested();

public slots:
    void showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourceList &resources);
    void showBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void hideAll();

private slots:
    void at_list_itemRemoved(QnNotificationItem* item);
    void at_item_actionTriggered(Qn::ActionId id, QnActionParameters* parameters);
private:
    QnNotificationListWidget *m_list;
    QGraphicsWidget* m_headerWidget;
    
};

#endif // NOTIFICATIONS_COLLECTION_ITEM_H
