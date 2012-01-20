#ifndef QN_IMAGE_BUTTON_WIDGET_H
#define QN_IMAGE_BUTTON_WIDGET_H

#include <QGraphicsWidget>
#include <QPixmap>
#include <ui/processors/clickable.h>

class VariantAnimator;

/**
 * A lightweight button widget that does not use style for painting. 
 */
class QnImageButtonWidget: public Clickable<QGraphicsWidget> {
    Q_OBJECT;

    typedef Clickable<QGraphicsWidget> base_type;

public:
    enum StateFlag {
        DEFAULT = 0,
        CHECKED = 0x1,
        HOVERED = 0x2,
        DISABLED = 0x4,
        FLAGS_MAX = 0x7
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag);

    QnImageButtonWidget(QGraphicsItem *parent = NULL);

    const QPixmap &pixmap(StateFlags flags) const;

    void setPixmap(StateFlags flags, const QPixmap &pixmap);

    StateFlags state() const { return m_state; }

    bool isCheckable() const { return m_checkable; }

    bool isChecked() const { return state() & CHECKED; }

    bool isHovered() const { return state() & HOVERED; }

    bool isDisabled() const { return state() & DISABLED; }

    bool isEnabled() const { return !isEnabled(); }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    qreal animationSpeed() const;

    void setAnimationSpeed(qreal animationSpeed);

public Q_SLOTS:
    void setCheckable(bool checkable);
    void setChecked(bool checked);
    void setEnabled(bool enabled = true);
    void setDisabled(bool disabled = false);
    inline void toggle() { setChecked(!isChecked()); }

Q_SIGNALS:
    void clicked(bool checked = false);
    void toggled(bool checked);
    void enabled();

protected:
    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) override;

    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    const QPixmap &actualPixmap(StateFlags flags);
    void updateState(StateFlags state);

private:
    friend class QnImageButtonHoverProgressAccessor;

    QPixmap m_pixmaps[FLAGS_MAX + 1];
    StateFlags m_state;
    bool m_checkable;

    VariantAnimator *m_animator;
    qreal m_hoverProgress;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnImageButtonWidget::StateFlags);

#endif // QN_IMAGE_BUTTON_WIDGET_H
