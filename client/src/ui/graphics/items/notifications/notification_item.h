#ifndef NOTIFICATION_ITEM_H
#define NOTIFICATION_ITEM_H

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>

class QnProxyLabel;
class QGraphicsLinearLayout;
class HoverFocusProcessor;

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

class QnNotificationToolTipWidget: public QnToolTipWidget {
    Q_OBJECT

    typedef QnToolTipWidget base_type;
public:
    QnNotificationToolTipWidget(QGraphicsItem *parent = 0);

    Q_SLOT void setThumbnail(const QImage &image);

    /**
     * Set tooltip text.
     * \param text                      New text for this tool tip's label.
     * \reimp
     */
    void setText(const QString &text);

    void setEnclosingGeometry(const QRectF &enclosingGeometry);

    //reimp
    void pointTo(const QPointF &pos);

    void updateTailPos();
protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;

private:
    QnProxyLabel* m_textLabel;
    QnProxyLabel* m_thumbnailLabel;
    QRectF m_enclosingGeometry;
    QPointF m_pointTo;
};

//TODO: #GDM rename to QnNotificationWidget
class QnNotificationItem: public QnClickableFrameWidget {
    Q_OBJECT
    typedef QnClickableFrameWidget base_type;

public:
    explicit QnNotificationItem(QGraphicsItem *parent = 0, Qt::WindowFlags flags = 0);
    virtual ~QnNotificationItem();

    QString text() const;

    void addActionButton(const QIcon &icon, const QString &tooltip, Qn::ActionId actionId,
                         const QnActionParameters &parameters = QnActionParameters(),
                         const qreal sizeMultiplier = 1.0, const bool isThumbnail = false);

    QColor color() const;


public slots:
    void setText(const QString &text);
    void setColor(const QColor &color);
    void setTooltipText(const QString &text);
    void setImage(const QImage &image);
    void setTooltipEnclosingGeometry(const QRectF& rect);

signals:
    void actionTriggered(Qn::ActionId actionId, const QnActionParameters &parameters);
    void imageChanged(const QImage& image);

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
    QImage m_image;

    QnNotificationToolTipWidget* m_tooltipWidget;
    HoverFocusProcessor* m_hoverProcessor;
    bool m_pendingPositionUpdate;
    bool m_instantPositionUpdate;
};

#endif // NOTIFICATION_ITEM_H
