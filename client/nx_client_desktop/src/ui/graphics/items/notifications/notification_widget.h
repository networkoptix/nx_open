#pragma once

#include <QtGui/QPainter>

#include <ui/common/notification_levels.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>
#include <ui/graphics/items/generic/graphics_tooltip_widget.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

class QnProxyLabel;
class HoverFocusProcessor;
class QnImageProvider;
class QGraphicsLinearLayout;

class QnNotificationToolTipWidget: public QnGraphicsToolTipWidget
{
    Q_OBJECT

    using base_type = QnGraphicsToolTipWidget;
public:
    QnNotificationToolTipWidget(QGraphicsItem* parent = nullptr);

    /** Rectangle where the tooltip should fit - in coordinates of list widget (parent's parent). */
    void setEnclosingGeometry(const QRectF& enclosingGeometry);

    virtual void updateTailPos() override;

signals:
    void closeTriggered();
    void buttonClicked(const QString& alias);

private:
    QRectF m_enclosingRect;
};

class QnNotificationWidget: public Clickable<GraphicsWidget>
{
    Q_OBJECT

    using base_type = Clickable<GraphicsWidget>;
public:
    explicit QnNotificationWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationWidget();

    QString text() const;

    using ActionType = nx::client::desktop::ui::action::IDType;
    using ParametersType = nx::client::desktop::ui::action::Parameters;

    void addActionButton(
        const QIcon& icon,
        ActionType actionId = nx::client::desktop::ui::action::NoAction,
        const ParametersType& parameters = ParametersType(),
        bool defaultAction = false);

    using ButtonHandler = std::function<void ()>;
    void addTextButton(
        const QIcon& icon,
        const QString& text,
        const ButtonHandler& handler);

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
    void actionTriggered(nx::client::desktop::ui::action::IDType actionId,
        const nx::client::desktop::ui::action::Parameters& parameters);
    void buttonClicked(const QString& alias);
    void linkActivated(const QString& link);

protected:
    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    virtual void clickedNotify(QGraphicsSceneMouseEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    void hideToolTip();
    void showToolTip();
    void updateLabelPalette();

private slots:
    void updateToolTipVisibility();
    void updateToolTipPosition();

    void at_loop_sound();

private:
    struct ActionData
    {
        ActionData(): action(nx::client::desktop::ui::action::NoAction) {}
        ActionData(nx::client::desktop::ui::action::IDType action) : action(action) {}
        ActionData(
            nx::client::desktop::ui::action::IDType action,
            const nx::client::desktop::ui::action::Parameters& params)
            :
            action(action),
            params(params)
        {
        }

        nx::client::desktop::ui::action::IDType action;
        nx::client::desktop::ui::action::Parameters params;
    };

    QList<ActionData> m_actions;
    int m_defaultActionIdx;
    QString m_soundPath;

    QGraphicsLinearLayout* m_verticalLayout;
    QGraphicsLinearLayout* m_primaryLayout;
    QnProxyLabel* m_textLabel;
    QnImageButtonWidget* m_closeButton;
    QnNotificationLevel::Value m_notificationLevel;
    QPointer<QnImageProvider> m_imageProvider;
    QColor m_color;

    QnNotificationToolTipWidget* m_tooltipWidget;
    HoverFocusProcessor* m_toolTipHoverProcessor;
    HoverFocusProcessor* m_hoverProcessor;
    bool m_pendingPositionUpdate;
    bool m_instantPositionUpdate;
    bool m_inToolTipPositionUpdate;
};
