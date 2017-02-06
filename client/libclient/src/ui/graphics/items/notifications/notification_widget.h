#pragma once

#include <QtGui/QPainter>

#include <ui/common/notification_levels.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>

class QnProxyLabel;
class QnClickableProxyLabel;
class QnFramedWidget;
class QGraphicsLinearLayout;
class HoverFocusProcessor;
class QnImageProvider;

class QnNotificationToolTipWidget: public Clickable<QnStyledTooltipWidget>
{
    Q_OBJECT
    typedef Clickable<QnStyledTooltipWidget> base_type;

public:
    QnNotificationToolTipWidget(QGraphicsItem* parent = nullptr);

    void ensureThumbnail(QnImageProvider* provider);

    QString text() const;

    /**
     * \param text                      New text for this tool tip's label.
     */
    void setText(const QString& text);

    /** Rectangle where the tooltip should fit - in coordinates of list widget (parent's parent). */
    void setEnclosingGeometry(const QRectF& enclosingGeometry);

    //reimp
    void pointTo(const QPointF& pos);
    virtual void updateTailPos() override;

signals:
    void thumbnailClicked();
    void closeTriggered();
    void buttonClicked(const QString& alias);

private slots:
    void at_thumbnailLabel_clicked(Qt::MouseButton button);

private:
    QGraphicsLinearLayout* m_layout;
    QnClickableProxyLabel* m_textLabel;
    QnClickableProxyLabel* m_thumbnailLabel;
    QRectF m_enclosingRect;
    QPointF m_pointTo;
};

class QnNotificationWidget: public Clickable<GraphicsWidget>
{
    Q_OBJECT
    typedef Clickable<GraphicsWidget> base_type;

public:
    explicit QnNotificationWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationWidget();

    QString text() const;

    void addActionButton(const QIcon& icon,
                         QnActions::IDType actionId = QnActions::NoAction,
                         const QnActionParameters& parameters = QnActionParameters(),
                         bool defaultAction = false);

    QnNotificationLevel::Value notificationLevel() const;
    void setNotificationLevel(QnNotificationLevel::Value notificationLevel);

    /**
     * \param rect Rectangle where all tooltips should fit, in parent(!) coordinates.
     */
    void setTooltipEnclosingRect(const QRectF& rect);

    void setImageProvider(QnImageProvider* provider);

    void setText(const QString& text);

    void setTooltipText(const QString& text);

    void setSound(const QString& soundPath, bool loop);

    virtual void setGeometry(const QRectF& geometry) override;

    void triggerDefaultAction(); //< emits actionTriggered with default action

signals:
    void notificationLevelChanged();
    void closeTriggered();
    void actionTriggered(QnActions::IDType actionId, const QnActionParameters& parameters);
    void buttonClicked(const QString& alias);
    void linkActivated(const QString& link);

protected:
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    virtual void clickedNotify(QGraphicsSceneMouseEvent* event) override;

private:
    void hideToolTip();
    void showToolTip();

private slots:
    void updateToolTipVisibility();
    void updateToolTipPosition();

    void at_loop_sound();

private:
    struct ActionData
    {
        ActionData(): action(QnActions::NoAction) {}
        ActionData(QnActions::IDType action) : action(action) {}
        ActionData(QnActions::IDType action, const QnActionParameters& params): action(action), params(params) {}

        QnActions::IDType action;
        QnActionParameters params;
    };

    QList<ActionData> m_actions;
    int m_defaultActionIdx;
    QString m_soundPath;

    QGraphicsLinearLayout* m_layout;
    QnProxyLabel* m_textLabel;
    QnImageButtonWidget* m_closeButton;
    QnNotificationLevel::Value m_notificationLevel;
    QnImageProvider* m_imageProvider;
    QColor m_color;

    QnNotificationToolTipWidget* m_tooltipWidget;
    HoverFocusProcessor* m_toolTipHoverProcessor;
    HoverFocusProcessor* m_hoverProcessor;
    bool m_pendingPositionUpdate;
    bool m_instantPositionUpdate;
    bool m_inToolTipPositionUpdate;
};
