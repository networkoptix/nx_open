#ifndef QN_IMAGE_BUTTON_WIDGET_H
#define QN_IMAGE_BUTTON_WIDGET_H

#include <QGraphicsWidget>
#include <QPixmap>
#include <ui/processors/clickable.h>
#include <ui/graphics/opengl/gl_functions.h>

class QAction;
class QIcon;

class VariantAnimator;
class QnTextureTransitionShaderProgram;

/**
 * A lightweight button widget that does not use style for painting. 
 */
class QnImageButtonWidget: public Clickable<QGraphicsWidget> {
    Q_OBJECT;
    Q_FLAGS(StateFlags StateFlag);
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable);
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled USER true);
    Q_PROPERTY(bool cached READ isCached WRITE setCached);
    Q_PROPERTY(qreal animationSpeed READ animationSpeed WRITE setAnimationSpeed);

    typedef Clickable<QGraphicsWidget> base_type;

public:
    enum StateFlag {
        DEFAULT = 0,        /**< Default button state. */
        CHECKED = 0x1,      /**< Button is checkable and is checked. */
        PRESSED = 0x2,      /**< Button is pressed. This is the state that the button enters when a mouse button is pressed over it, and leaves when the mouse button is released. */
        HOVERED = 0x4,      /**< Button is hovered over. */
        DISABLED = 0x8,     /**< Button is disabled. */
        FLAGS_MAX = 0xF
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag);

    QnImageButtonWidget(QGraphicsItem *parent = NULL);

    const QPixmap &pixmap(StateFlags flags) const;

    void setPixmap(StateFlags flags, const QPixmap &pixmap);

    void setIcon(const QIcon &icon);

    StateFlags state() const { return m_state; }

    bool isCheckable() const { return m_checkable; }

    bool isChecked() const { return state() & CHECKED; }

    bool isHovered() const { return state() & HOVERED; }

    bool isDisabled() const { return !isEnabled(); }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    qreal animationSpeed() const;

    void setAnimationSpeed(qreal animationSpeed);

    void setDefaultAction(QAction *action);

    QAction *defaultAction() const;

    bool isCached() const;

    void setCached(bool cached);

public Q_SLOTS:
    void setCheckable(bool checkable);
    void setChecked(bool checked);
    void setDisabled(bool disabled = false);
    inline void toggle() { setChecked(!isChecked()); }
    void click();

Q_SIGNALS:
    void clicked(bool checked = false);
    void toggled(bool checked);
    void enabled();

protected:
    virtual void clickedNotify(QGraphicsSceneMouseEvent *event) override;
    virtual void pressedNotify(QGraphicsSceneMouseEvent *event) override;
    virtual void releasedNotify(QGraphicsSceneMouseEvent *event) override;

    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual bool event(QEvent *event) override;

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget);

protected:
    void updateState(StateFlags state);

    void ensurePixmapCache() const;
    void invalidatePixmapCache();
    StateFlags displayState(StateFlags flags) const;

private:
    friend class QnImageButtonHoverProgressAccessor;

    QPixmap m_pixmaps[FLAGS_MAX + 1];
    mutable QPixmap m_pixmapCache[FLAGS_MAX + 1];
    mutable bool m_pixmapCacheValid;

    StateFlags m_state;
    bool m_checkable;
    bool m_cached;

    VariantAnimator *m_animator;
    qreal m_hoverProgress;

    QAction *m_action;
    QSharedPointer<QnTextureTransitionShaderProgram> m_shader;
    QScopedPointer<QnGlFunctions> m_gl;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnImageButtonWidget::StateFlags);


class QnZoomingImageButtonWidget: public QnImageButtonWidget {
    Q_OBJECT;

    typedef QnImageButtonWidget base_type;

public:
    QnZoomingImageButtonWidget(QGraphicsItem *parent = NULL);

    qreal scaleFactor() const {
        return m_scaleFactor;
    }

    void setScaleFactor(qreal scaleFactor) {
        m_scaleFactor = scaleFactor;
    }

protected:
    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget) override;

    bool isScaledState(StateFlags state);

private:
    qreal m_scaleFactor;
};

#endif // QN_IMAGE_BUTTON_WIDGET_H
