#ifndef NOTIFICATIONS_COLLECTION_ITEM_H
#define NOTIFICATIONS_COLLECTION_ITEM_H

#include <QtGui/QGraphicsItem>

#include <ui/graphics/items/generic/simple_frame_widget.h>
#include <health/system_health.h>

class QnNotificationsCollectionItem : public QnSimpleFrameWidget
{
    Q_OBJECT

    typedef QnSimpleFrameWidget base_type;
public:
    explicit QnNotificationsCollectionItem(QGraphicsItem *parent = 0);
    

    bool addSystemHealthEvent(QnSystemHealth::MessageType message);

signals:
    
public slots:
    
};

#endif // NOTIFICATIONS_COLLECTION_ITEM_H
