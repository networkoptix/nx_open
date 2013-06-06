#ifndef NOTIFICATION_ITEM_H
#define NOTIFICATION_ITEM_H

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>

class QnProxyLabel;
class QGraphicsLinearLayout;
class QnToolTipWidget;
class HoverFocusProcessor;

class QnNotificationItem: public QnClickableFrameWidget {
    Q_OBJECT
    typedef QnClickableFrameWidget base_type;

public:
    explicit QnNotificationItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationItem();

    QString text() const;
    void setText(const QString &text);

    void setTooltipText(const QString &text);

    void addActionButton(const QIcon &icon, const QString &tooltip, Qn::ActionId actionId,
                         const QnActionParameters &parameters = QnActionParameters(), const qreal sizeMultiplier = 1.0);

    QColor color() const;
    void setColor(const QColor &color);

signals:
    void actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void hideToolTip();
    void showToolTip();

private slots:
    void updateToolTipVisibility();
    void updateToolTipPosition();

    void at_button_clicked();

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

    QList<ActionData> m_actions;

    QGraphicsLinearLayout* m_layout;
    QnProxyLabel* m_textLabel;
    QColor m_color;

    QnToolTipWidget* m_tooltipItem;
    HoverFocusProcessor* m_hoverProcessor;
    bool m_pendingPositionUpdate;
    bool m_instantPositionUpdate;
};

#endif // NOTIFICATION_ITEM_H
