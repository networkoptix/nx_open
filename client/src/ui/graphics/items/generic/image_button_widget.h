#ifndef QN_IMAGE_BUTTON_WIDGET_H
#define QN_IMAGE_BUTTON_WIDGET_H

#include <boost/array.hpp>

#include <QtGui/QPixmap>
#include <QtGui/QOpenGLFunctions>

#include <ui/processors/clickable.h>
#include <ui/animation/animated.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>

class QAction;
class QMenu;
class QIcon;
class QGLWidget;

class VariantAnimator;
class QnTextureTransitionShaderProgram;

/**
 * A lightweight button widget that does not use styles for painting.
 */
class QnImageButtonWidget: public Animated<Clickable<GraphicsWidget> >, protected QOpenGLFunctions {
    Q_OBJECT
    Q_FLAGS(StateFlags StateFlag)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled USER true)
    Q_PROPERTY(bool cached READ isCached WRITE setCached)
    Q_PROPERTY(qreal animationSpeed READ animationSpeed WRITE setAnimationSpeed)

    typedef Animated<Clickable<GraphicsWidget> > base_type;

public:
    enum StateFlag {
        DEFAULT = 0,        /**< Default button state. */
        CHECKED = 0x1,      /**< Button is checkable and is checked. */
        PRESSED = 0x2,      /**< Button is pressed. This is the state that the button enters when a mouse button is pressed over it, and leaves when the mouse button is released. */
        HOVERED = 0x4,      /**< Button is hovered over. */
        DISABLED = 0x8,     /**< Button is disabled. */
        FLAGS_MAX = 0xF
    };
    Q_DECLARE_FLAGS(StateFlags, StateFlag)

    QnImageButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnImageButtonWidget();

    const QPixmap &pixmap(StateFlags flags) const;
    void setPixmap(StateFlags flags, const QPixmap &pixmap);
    void setIcon(const QIcon &icon);

    bool isCheckable() const { return m_checkable; }
    Q_SLOT void setCheckable(bool checkable);

    StateFlags state() const { return m_state; }
    bool isHovered() const { return state() & HOVERED; }
    bool isChecked() const { return state() & CHECKED; }
    bool isPressed() const { return state() & PRESSED; }
    bool isDisabled() const { return !isEnabled(); }
    Q_SLOT void setChecked(bool checked = true);
    Q_SLOT void setPressed(bool pressed = true);
    Q_SLOT void setDisabled(bool disabled = true);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    qreal animationSpeed() const;
    void setAnimationSpeed(qreal animationSpeed);

    void setDefaultAction(QAction *action);
    QAction *defaultAction() const;

    bool isCached() const;
    void setCached(bool cached);

    void setFixedSize(qreal size);
    void setFixedSize(qreal width, qreal height);
    void setFixedSize(const QSizeF &size);

public slots:
    void toggle() { setChecked(!isChecked()); }
    void click();

signals:
    void clicked(bool checked = false);
    void toggled(bool checked);
    void pressed();
    void released();
    void stateChanged();

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

    void ensurePixmapCache() const;
    void invalidatePixmapCache();
    StateFlags validPixmapState(StateFlags flags) const;

    bool skipHoverEvent(QGraphicsSceneHoverEvent *event);
    bool skipMenuEvent(QGraphicsSceneMouseEvent *event);

    void clickInternal(QGraphicsSceneMouseEvent *event);

private:
    friend class QnImageButtonHoverProgressAccessor;

    boost::array<QPixmap, FLAGS_MAX + 1> m_pixmaps;
    mutable boost::array<QPixmap, FLAGS_MAX + 1> m_pixmapCache;
    mutable bool m_pixmapCacheValid;

    StateFlags m_state;
    bool m_checkable;
    bool m_cached;
    int m_skipNextHoverEvents;
    QPoint m_nextHoverEventPos;
    int m_skipNextMenuEvents;
    QPoint m_nextMenuEventPos;

    VariantAnimator *m_animator;
    qreal m_hoverProgress;

    QAction *m_action;
    bool m_actionIconOverridden;

    QSharedPointer<QnTextureTransitionShaderProgram> m_shader;
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

    qreal rotationSpeed() const {
        return m_rotationSpeed;
    }

    void setRotationSpeed(qreal rotationSpeed) {
        m_rotationSpeed = rotationSpeed;
    }

protected:
    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) override;

    virtual void tick(int deltaMSecs) override;

private:
    qreal m_rotationSpeed;
    qreal m_rotation;
};



/**
 * An image button widget that can show text and draw a frame.
 */
class QnTextButtonWidget: public Framed<QnImageButtonWidget> {
    Q_OBJECT
    Q_PROPERTY(qreal frameWidth READ frameWidth WRITE setFrameWidth)
    Q_PROPERTY(Qn::FrameShape frameShape READ frameShape WRITE setFrameShape)
    Q_PROPERTY(Qt::PenStyle frameStyle READ frameStyle WRITE setFrameStyle)
    Q_PROPERTY(QBrush frameBrush READ frameBrush WRITE setFrameBrush)
    Q_PROPERTY(QColor frameColor READ frameColor WRITE setFrameColor)
    Q_PROPERTY(QBrush windowBrush READ windowBrush WRITE setWindowBrush)
    Q_PROPERTY(QColor windowColor READ windowColor WRITE setWindowColor)
    Q_PROPERTY(QBrush textBrush READ textBrush WRITE setTextBrush)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(qreal relativeFrameWidth READ relativeFrameWidth WRITE setRelativeFrameWidth)
    typedef Framed<QnImageButtonWidget> base_type;

public:
    QnTextButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);

    const QString &text() const;
    void setText(const QString &text);

    QBrush textBrush() const;
    void setTextBrush(const QBrush &textBrush);

    QColor textColor() const;
    void setTextColor(const QColor &textColor);

    qreal relativeFrameWidth() const;
    void setRelativeFrameWidth(qreal relativeFrameWidth);

    qreal stateOpacity(StateFlags stateFlags) const;
    void setStateOpacity(StateFlags stateFlags, qreal opacity);

    virtual void setGeometry(const QRectF &geometry) override;

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) override;

    virtual void changeEvent(QEvent *event) override;

    void invalidatePixmap();
    void ensurePixmap();

protected:
    StateFlags validOpacityState(StateFlags flags) const;

private:
    QString m_text;
    bool m_pixmapValid;
    qreal m_relativeFrameWidth;
    boost::array<qreal, FLAGS_MAX + 1> m_opacities;
};

#endif // QN_IMAGE_BUTTON_WIDGET_H
