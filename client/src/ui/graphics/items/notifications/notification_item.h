#ifndef NOTIFICATION_ITEM_H
#define NOTIFICATION_ITEM_H

#include <ui/actions/actions.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widget.h>

class QnProxyLabel;
class QnImageButtonWidget;
class QGraphicsLinearLayout;
class QnActionParameters;

class QnNotificationItem: public QnClickableWidget {
    Q_OBJECT

    typedef QnClickableWidget base_type;
public:
    explicit QnNotificationItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationItem();

    QString text() const;
    void setText(const QString &text);

    void setIcon(const QIcon &icon);

    QColor color() const;
    void setColor(const QColor &color);

    void setAction(Qn::ActionId action, QnActionParameters* parameters = NULL) {
        m_action = action;
        m_parameters = parameters;
    }

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

signals:
    void actionTriggered(Qn::ActionId action, QnActionParameters* parameters);

private slots:
    void at_image_clicked();

private:
    QnProxyLabel* m_textLabel;
    QnImageButtonWidget *m_image;
    QColor m_color;
    Qn::ActionId m_action;
    QnActionParameters *m_parameters;
};

#endif // NOTIFICATION_ITEM_H
