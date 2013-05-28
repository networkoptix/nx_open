#ifndef NOTIFICATIONS_COLLECTION_ITEM_H
#define NOTIFICATIONS_COLLECTION_ITEM_H

#include <QtGui/QGraphicsItem>

#include <ui/graphics/items/standard/graphics_widget.h>

#include <health/system_health.h>

class QGraphicsLinearLayout;
class QnNotificationListWidget;
class QnNotificationItem;

class QnNotificationsCollectionItem : public GraphicsWidget
{
    Q_OBJECT

    typedef GraphicsWidget base_type;
public:
    explicit QnNotificationsCollectionItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationsCollectionItem();

    bool addSystemHealthEvent(QnSystemHealth::MessageType message);

    QRectF headerGeometry() const;

private slots:
    void at_list_itemRemoved(QnNotificationItem* item);
private:
    QnNotificationListWidget *m_list;
    QGraphicsWidget* m_headerWidget;
    
};

#endif // NOTIFICATIONS_COLLECTION_ITEM_H
