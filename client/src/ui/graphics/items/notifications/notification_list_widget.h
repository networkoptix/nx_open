#ifndef NOTIFICATION_LIST_WIDGET_H
#define NOTIFICATION_LIST_WIDGET_H

#include <ui/graphics/items/standard/graphics_widget.h>

class QnNotificationItem;

class QnNotificationListWidget : public GraphicsWidget
{
    Q_OBJECT
    typedef GraphicsWidget base_type;
public:
    explicit QnNotificationListWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags flags = 0);
    ~QnNotificationListWidget();

    void addItem(QnNotificationItem *item);
private slots:
    void at_item_geometryChanged();

private:
    QList<QnNotificationItem*> m_items;
};

#endif // NOTIFICATION_LIST_WIDGET_H
