#ifndef NOTIFICATION_ITEM_H
#define NOTIFICATION_ITEM_H

#include <ui/graphics/items/standard/graphics_widget.h>

class GraphicsLabel;
class QnImageButtonWidget;
class QGraphicsLinearLayout;

class QnNotificationItem: public GraphicsWidget {
    Q_OBJECT

    typedef GraphicsWidget base_type;
public:
    explicit QnNotificationItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationItem();

    QString text() const;
    void setText(const QString &text);

    QImage image() const;
    void setImage(const QImage& image);

    QColor color() const;
    void setColor(const QColor &color);

signals:
    void imageClicked();

private:
    GraphicsLabel* m_textLabel;
    QnImageButtonWidget *m_image;
    QGraphicsLinearLayout* m_layout;
};

#endif // NOTIFICATION_ITEM_H
