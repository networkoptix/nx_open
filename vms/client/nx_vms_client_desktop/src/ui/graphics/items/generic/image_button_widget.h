#pragma once

#include <array>

#include <QtGui/QPixmap>
#include <QtGui/QOpenGLBuffer>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLVertexArrayObject>

#include <ui/processors/clickable.h>
#include <ui/animation/animated.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>

class QAction;
class QMenu;
class QIcon;
class QGLWidget;

class VariantAnimator;

/**
 * A lightweight button widget that does not use styles for painting.
 */
class QnImageButtonWidget: public Animated<Clickable<GraphicsWidget> > {
    Q_OBJECT
    Q_FLAGS(StateFlags StateFlag)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled USER true)
    Q_PROPERTY(qreal animationSpeed READ animationSpeed WRITE setAnimationSpeed)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)

    typedef Animated<Clickable<GraphicsWidget> > base_type;

public:
    enum StateFlag
    {
        Default = 0,        /**< Default button state. */
        Checked = 0x1,      /**< Button is checkable and is checked. */
        Pressed = 0x2,      /**< Button is pressed. This is the state that the button enters when a mouse button is pressed over it, and leaves when the mouse button is released. */
        Hovered = 0x4,      /**< Button is hovered over. */
        Disabled = 0x8,     /**< Button is disabled. */
        MaxState = 0xF
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag)

    QnImageButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnImageButtonWidget();

    QIcon icon() const;
    void setIcon(const QIcon &icon);

    bool isCheckable() const { return m_checkable; }
    Q_SLOT void setCheckable(bool checkable);

    StateFlags state() const { return m_state; }
    bool isHovered() const { return state() & Hovered; }
    bool isChecked() const { return state() & Checked; }
    bool isPressed() const { return state() & Pressed; }
    bool isDisabled() const { return !isEnabled(); }
    Q_SLOT void setChecked(bool checked = true);
    Q_SLOT void setPressed(bool pressed = true);
    Q_SLOT void setDisabled(bool disabled = true);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    qreal animationSpeed() const;
    void setAnimationSpeed(qreal animationSpeed);

    void setDefaultAction(QAction *action);
    QAction *defaultAction() const;

    void setFixedSize(qreal size);
    void setFixedSize(qreal width, qreal height);
    void setFixedSize(const QSizeF &size);

    QMarginsF imageMargins() const;
    void setImageMargins(const QMarginsF &margins);

    /** If the button size can be changed in runtime. */
    bool isDynamic() const;
    /**
     * Allow or forbid dynamic size changes in runtime.
     * Must be set before first paint because used in the VAO generation.
     */
    void setDynamic(bool value);
public slots:
    void toggle() { setChecked(!isChecked()); }
    void click();

signals:
    void clicked(bool checked = false);
    void toggled(bool checked);
    void pressed();
    void released();
    void stateChanged();
    void iconChanged();

protected:
    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) override;
    virtual void pressedNotify(QGraphicsSceneMouseEvent *event) override;
    virtual void releasedNotify(QGraphicsSceneMouseEvent *event) override;

    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    virtual bool event(QEvent *event) override;

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect);

protected:
    void updateState(StateFlags state);
    void updateFromDefaultAction();

    StateFlags validPixmapState(StateFlags flags) const;

    void initializeVao(const QRectF &rect);
    void updateVao(const QRectF &rect);

    bool skipHoverEvent(QGraphicsSceneHoverEvent *event);

    void clickInternal(QGraphicsSceneMouseEvent *event);

    // TODO: #Elric get rid of these, leave only icon.
    const QPixmap &pixmap(StateFlags flags) const;
    void setPixmap(StateFlags flags, const QPixmap &pixmap);

private:
    friend class QnImageButtonHoverProgressAccessor;

    std::array<QPixmap, MaxState + 1> m_pixmaps;

    StateFlags m_state;
    bool m_checkable;
    bool m_dynamic;
    int m_skipNextHoverEvents;
    QPoint m_nextHoverEventPos;

    VariantAnimator *m_animator;
    qreal m_hoverProgress;

    QAction *m_action;
    bool m_actionIconOverridden;

    bool m_initialized;
    QOpenGLVertexArrayObject m_verticesStatic;
    QOpenGLVertexArrayObject m_verticesTransition;
    QOpenGLBuffer m_positionBufferStatic;
    QOpenGLBuffer m_positionBufferTransition;
    QOpenGLBuffer m_textureBufferStatic;
    QOpenGLBuffer m_textureBufferTransition;

    QMarginsF m_imageMargins;

    QIcon m_icon;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnImageButtonWidget::StateFlags)


/**
 * An image button widget that rotates when checked.
 */
class QnRotatingImageButtonWidget: public QnImageButtonWidget, public AnimationTimerListener {
    Q_OBJECT
    typedef QnImageButtonWidget base_type;
public:
    QnRotatingImageButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);

    qreal rotationSpeed() const;
    void setRotationSpeed(qreal rotationSpeed);

protected:
    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) override;
    virtual void tick(int deltaMSecs) override;

private:
    qreal m_rotationSpeed;
    qreal m_rotation;
};
