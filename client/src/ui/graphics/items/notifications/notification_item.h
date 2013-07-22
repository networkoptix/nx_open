#ifndef QN_NOTIFICATION_ITEM_H
#define QN_NOTIFICATION_ITEM_H

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>

class QnProxyLabel;
class QnClickableProxyLabel;
class QnSimpleFrameWidget;
class QGraphicsLinearLayout;
class HoverFocusProcessor;
class QnImageProvider;

/**
 * An image button widget that displays thumbnail behind the button.
 */
class QnThumbnailImageButtonWidget: public QnImageButtonWidget {
    Q_OBJECT

    typedef QnImageButtonWidget base_type;

public:
    QnThumbnailImageButtonWidget(QGraphicsItem *parent = NULL):
        base_type(parent) {}

    const QImage& thumbnail() const {
        return m_thumbnail;
    }

    Q_SLOT void setThumbnail(const QImage &image) {
        m_thumbnail = image;
    }

protected:
    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) override {
        if (!m_thumbnail.isNull())
            painter->drawImage(rect, m_thumbnail);
        base_type::paint(painter, startState, endState, progress, widget, rect);
    }
private:
    QImage m_thumbnail;
};


class QnNotificationToolTipWidget: public QnStyledTooltipWidget {
    Q_OBJECT

    typedef QnStyledTooltipWidget base_type;
public:
    QnNotificationToolTipWidget(QGraphicsItem *parent = 0);

    void ensureThumbnail(QnImageProvider* provider);

    QString text() const;

    /**
     * \param text                      New text for this tool tip's label.
     */
    void setText(const QString &text);

    /** Rectangle where the tooltip should fit - in coordinates of list widget (parent's parent). */
    void setEnclosingGeometry(const QRectF &enclosingGeometry);

    //reimp
    void pointTo(const QPointF &pos);
    virtual void updateTailPos() override;

signals:
    void thumbnailClicked();

private slots:
    void at_provider_imageChanged(const QImage &image);

private:
    QnProxyLabel* m_textLabel;
    QnClickableProxyLabel* m_thumbnailLabel;
    QRectF m_enclosingRect;
    QPointF m_pointTo;
};


//TODO: #GDM rename to QnNotificationWidget
class QnNotificationItem: public Clickable<QnSimpleFrameWidget> {
    Q_OBJECT
    typedef Clickable<QnSimpleFrameWidget> base_type;

public:
    explicit QnNotificationItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationItem();

    QString text() const;

    void addActionButton(const QIcon &icon, const QString &tooltip, Qn::ActionId actionId,
                         const QnActionParameters &parameters = QnActionParameters(),
                         bool defaultAction = false);

    static QColor notificationColor(Qn::NotificationLevel level);

    Qn::NotificationLevel notificationLevel() const;
    void setNotificationLevel(Qn::NotificationLevel notificationLevel);

    /** 
     * \param rect                      Rectangle where all tooltips should fit, in parent(!) coordinates. 
     */
    void setTooltipEnclosingRect(const QRectF &rect);

    void setImageProvider(QnImageProvider *provider);

    void setText(const QString &text);
    
    void setTooltipText(const QString &text);

    virtual void setGeometry(const QRectF &geometry) override;

signals:
    void notificationLevelChanged();
    void closeTriggered();
    void actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void clicked(Qt::MouseButton button) override;

private:
    void hideToolTip();
    void showToolTip();

private slots:
    void updateToolTipVisibility();
    void updateToolTipPosition();
    void updateOverlayVisibility(bool animate = true);
    void updateOverlayGeometry();
    void updateOverlayColor();
    void updateCloseButtonGeometry();

    void at_button_clicked();
    void at_thumbnail_clicked();

private:
    struct ActionData {
        ActionData(): action(Qn::NoAction){}
        ActionData(Qn::ActionId action): action(action){}
        ActionData(Qn::ActionId action, const QnActionParameters &params): action(action), params(params){}

        Qn::ActionId action;
        QnActionParameters params;
    };

    QList<ActionData> m_actions;
    int m_defaultActionIdx;

    QGraphicsLinearLayout *m_layout;
    QnProxyLabel *m_textLabel;
    QnImageButtonWidget *m_closeButton;
    QColor m_color;
    Qn::NotificationLevel m_notificationLevel;
    QnImageProvider* m_imageProvider;

    QnSimpleFrameWidget *m_overlayWidget;

    QnNotificationToolTipWidget *m_tooltipWidget;
    HoverFocusProcessor *m_toolTipHoverProcessor;
    HoverFocusProcessor *m_hoverProcessor;
    bool m_pendingPositionUpdate;
    bool m_instantPositionUpdate;
    bool m_inToolTipPositionUpdate;
};

#endif // QN_NOTIFICATION_ITEM_H
