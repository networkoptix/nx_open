#ifndef NOTIFICATIONS_COLLECTION_ITEM_H
#define NOTIFICATIONS_COLLECTION_ITEM_H

#include <QtGui/QGraphicsItem>

#include <ui/graphics/items/standard/graphics_widget.h>

#include <health/system_health.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;

class QnNotificationsCollectionItem : public GraphicsWidget
{
    Q_OBJECT

    typedef GraphicsWidget base_type;
public:
    explicit QnNotificationsCollectionItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationsCollectionItem();

    bool addSystemHealthEvent(QnSystemHealth::MessageType message);

private:
    QnNotificationListWidget *m_list;
    
};

#endif // NOTIFICATIONS_COLLECTION_ITEM_H
