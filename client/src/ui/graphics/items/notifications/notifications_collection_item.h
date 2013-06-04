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

    QRectF headerGeometry() const;

public slots:
    void showSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr &resource);
    void showBusinessAction(const QnAbstractBusinessActionPtr& businessAction);
    void hideAll();

private slots:
    void at_settingsButton_clicked();
    void at_eventLogButton_clicked();
    void at_list_itemRemoved(QnNotificationItem* item);
    void at_item_actionTriggered(QnNotificationItem* item);
private:
    struct ActionData {
        ActionData():
            action(Qn::NoAction){}
        ActionData(Qn::ActionId action):
            action(action){}
        ActionData(Qn::ActionId action, const QnActionParameters &params):
            action(action), params(params){}

        Qn::ActionId action;
        QnActionParameters params;
    };

    QnNotificationListWidget *m_list;
    QGraphicsWidget* m_headerWidget;
    QMap<QnNotificationItem*, ActionData> m_actionDataByItem;
    
};

#endif // NOTIFICATIONS_COLLECTION_ITEM_H
