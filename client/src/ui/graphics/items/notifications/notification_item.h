#ifndef NOTIFICATION_ITEM_H
#define NOTIFICATION_ITEM_H

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widget.h>

class GraphicsLabel;
class QnImageButtonWidget;
class QGraphicsLinearLayout;

class QnNotificationItem: public QnClickableWidget {
    Q_OBJECT

    typedef QnClickableWidget base_type;
public:
    explicit QnNotificationItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationItem();

    QString text() const;
    void setText(const QString &text);

    QString iconPath() const;
    void setIconPath(const QString& iconPath);

    void setIcon(const QIcon &icon);

    QColor color() const;
    void setColor(const QColor &color);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void imageClicked();

private:
    GraphicsLabel* m_textLabel;
    QnImageButtonWidget *m_image;
    QString m_iconPath;
    QColor m_color;
};

#endif // NOTIFICATION_ITEM_H
