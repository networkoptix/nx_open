#include "image_button_widget.h"
#include <cassert>
#include <QPainter>
#include <QIcon>
#include <QAction>
#include <QStyle>
#include <QGLContext>
#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>
#include <ui/animation/accessor.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/shaders/texture_transition_shader_program.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/common/scene_utility.h>

namespace {
    bool checkPixmapGroupRole(QnImageButtonWidget::StateFlags *flags) {
        bool result = true;

        if(*flags < 0 || *flags > QnImageButtonWidget::FLAGS_MAX) {
            qnWarning("Invalid pixmap flags '%1'.", static_cast<int>(*flags));
            *flags = 0;
            result = false;
        }

        return result;
    }

    QPixmap bestPixmap(const QIcon &icon, QIcon::Mode mode, QIcon::State state) {
        QList<QSize> sizes = icon.availableSizes(mode, state);
        if(sizes.empty())
            return QPixmap();

        QSize bestSize(0, 0);
        foreach(const QSize &size, sizes)
            if(size.width() > bestSize.width())
                bestSize = size;

        return icon.pixmap(bestSize, mode, state);
    }

    typedef QnGlContextData<QnTextureTransitionShaderProgram> QnTextureTransitionShaderProgramStorage;
    Q_GLOBAL_STATIC(QnTextureTransitionShaderProgramStorage, qn_textureTransitionShaderProgramStorage);

} // anonymous namespace

class QnImageButtonHoverProgressAccessor: public AbstractAccessor {
    virtual QVariant get(const QObject *object) const override {
        return static_cast<const QnImageButtonWidget *>(object)->m_hoverProgress;
    }

    virtual void set(QObject *object, const QVariant &value) const override {
        QnImageButtonWidget *widget = static_cast<QnImageButtonWidget *>(object);
        if(qFuzzyCompare(widget->m_hoverProgress, value.toReal()))
            return;

        widget->m_hoverProgress = value.toReal();
        widget->update();
    }
};

QnImageButtonWidget::QnImageButtonWidget(QGraphicsItem *parent):
    base_type(parent),
    m_pixmapCacheValid(false),
    m_checkable(false),
    m_cached(false),
    m_skipNextHoverEvents(false),
    m_state(0),
    m_hoverProgress(0.0),
    m_action(NULL)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);
    setAcceptsHoverEvents(true);
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton));

    m_animator = new VariantAnimator(this);
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new QnImageButtonHoverProgressAccessor());

    /* When hovering over a button, a cursor should always change to arrow pointer. */
    setCursor(Qt::ArrowCursor);

    /* Perform handler-based initialization. */
    itemChange(ItemSceneHasChanged, QVariant::fromValue<QGraphicsScene *>(scene()));

    QEvent styleChange(QEvent::StyleChange);
    event(&styleChange);
}

QnImageButtonWidget::~QnImageButtonWidget() {
    return;
}

const QPixmap &QnImageButtonWidget::pixmap(StateFlags flags) const {
    if(!m_cached) {
        return m_pixmaps[displayState(flags)];
    } else {
        ensurePixmapCache();

        return m_pixmapCache[displayState(flags)];
    }
}

void QnImageButtonWidget::setPixmap(StateFlags flags, const QPixmap &pixmap) {
    if(!checkPixmapGroupRole(&flags))
        return;

    m_pixmaps[flags] = pixmap;

    invalidatePixmapCache();
    update();
}

void QnImageButtonWidget::setIcon(const QIcon &icon) {
    for(int i = 0; i <= FLAGS_MAX; i++)
        m_pixmaps[i] = QPixmap();

    setPixmap(0,                    bestPixmap(icon, QIcon::Normal, QIcon::Off));
    setPixmap(HOVERED,              bestPixmap(icon, QIcon::Active, QIcon::Off));
    setPixmap(DISABLED,             bestPixmap(icon, QIcon::Disabled, QIcon::Off));
    setPixmap(PRESSED,              bestPixmap(icon, QIcon::Selected, QIcon::Off));

    setPixmap(CHECKED,              bestPixmap(icon, QIcon::Normal, QIcon::On));
    setPixmap(CHECKED | HOVERED,    bestPixmap(icon, QIcon::Active, QIcon::On));
    setPixmap(CHECKED | DISABLED,   bestPixmap(icon, QIcon::Disabled, QIcon::On));
    setPixmap(CHECKED | PRESSED,    bestPixmap(icon, QIcon::Selected, QIcon::On));
}

void QnImageButtonWidget::setCheckable(bool checkable) {
    if (m_checkable == checkable)
        return;

    m_checkable = checkable;
    if (!m_checkable)
        updateState(m_state & ~CHECKED);
    update();
}

void QnImageButtonWidget::setChecked(bool checked) {
    if (!m_checkable || checked == isChecked())
        return;

    updateState(checked ? (m_state | CHECKED) : (m_state & ~CHECKED));
    update();
}

void QnImageButtonWidget::setDisabled(bool disabled) {
    setEnabled(!disabled);
}

void QnImageButtonWidget::setPressed(bool pressed) {
    if(pressed) {
        updateState(m_state | PRESSED);
        updateState(m_state & ~HOVERED);
    } else {
        updateState(m_state & ~PRESSED);
    }
}

qreal QnImageButtonWidget::animationSpeed() const {
    return m_animator->speed();
}

void QnImageButtonWidget::setAnimationSpeed(qreal animationSpeed) {
    m_animator->setSpeed(animationSpeed);
}

void QnImageButtonWidget::click() {
    if(m_action != NULL)
        m_action->trigger();
    else
        toggle();
    emit clicked(isChecked());
}

void QnImageButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    if(m_shader.isNull()) {
        m_shader = qn_textureTransitionShaderProgramStorage()->get();
        m_gl.reset(new QnGlFunctions());
    }

    StateFlags hoverState = m_state | HOVERED;
    StateFlags normalState = m_state & ~HOVERED;
    paint(painter, hoverState, normalState, m_hoverProgress, checked_cast<QGLWidget *>(widget));
}

void QnImageButtonWidget::paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget) {
    painter->beginNativePainting();
    glPushAttrib(GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT); /* Push current color and blending-related options. */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_gl->glActiveTexture(GL_TEXTURE1);
    widget->bindTexture(pixmap(endState));
    m_gl->glActiveTexture(GL_TEXTURE0);
    widget->bindTexture(pixmap(startState));
    m_shader->bind();
    m_shader->setProgress(progress);
    m_shader->setTexture0(0);
    m_shader->setTexture1(1);

    QRectF rect = this->rect();

    glBegin(GL_QUADS);
    glColor(1.0, 1.0, 1.0, painter->opacity());
    glTexCoord(0.0, 1.0);
    glVertex(rect.topLeft());
    glTexCoord(1.0, 1.0);
    glVertex(rect.topRight());
    glTexCoord(1.0, 0.0);
    glVertex(rect.bottomRight());
    glTexCoord(0.0, 0.0);
    glVertex(rect.bottomLeft());
    glEnd();

    m_shader->release();

    glPopAttrib();
    painter->endNativePainting();
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *event) {
    if(isDisabled())
        return;

    QWeakPointer<QObject> self(this);

    click();

    if(self && m_action && m_action->menu() && !skipMenuEvent(event)) {
        QGraphicsView *view = qobject_cast<QGraphicsView *>(event->widget() ? event->widget()->parent() : NULL);
        if(!view) {
            qnWarning("No graphics view in scope, cannot show menu for an image button widget.");
            return;
        }

        QMenu *menu = m_action->menu();
        QPoint pos = view->mapToGlobal(view->mapFromScene(mapToScene(rect().bottomLeft())));

        menu->exec(pos);

        /* Cannot use QMenu::setNoReplayFor, as it will block clicks events for the whole scene. 
         * This is why we resort to nasty hacks with mouse position compares. */
        m_skipNextMenuEvents = 1;
        m_nextMenuEventPos = QCursor::pos();
    }
}

void QnImageButtonWidget::pressedNotify(QGraphicsSceneMouseEvent *) {
    setPressed(true);
}

void QnImageButtonWidget::releasedNotify(QGraphicsSceneMouseEvent *event) {
    setPressed(false);

    /* Next hover events that we will receive are enter and move converted from 
     * release event, skip them. */ 
    m_skipNextHoverEvents = 2;
    m_nextHoverEventPos = event->screenPos();
}

bool QnImageButtonWidget::skipHoverEvent(QGraphicsSceneHoverEvent *event) {
    if(m_skipNextHoverEvents) {
        if(event->screenPos() == m_nextHoverEventPos) {
            m_skipNextHoverEvents--;
            return true;
        } else {
            m_skipNextHoverEvents = 0;
            return false;
        }
    } else {
        return false;
    }
}

bool QnImageButtonWidget::skipMenuEvent(QGraphicsSceneMouseEvent *event) {
    if(m_skipNextMenuEvents) {
        if(event->screenPos() == m_nextMenuEventPos) {
            m_skipNextMenuEvents--;
            return true;
        } else {
            m_skipNextMenuEvents = 0;
            return false;
        }
    } else {
        return false;
    }
}

void QnImageButtonWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    if(skipHoverEvent(event))
        return;

    updateState(m_state | HOVERED);

    base_type::hoverEnterEvent(event);
}

void QnImageButtonWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    if(skipHoverEvent(event))
        return;

    updateState(m_state | HOVERED); /* In case we didn't receive the hover enter event. */

    base_type::hoverMoveEvent(event);
}

void QnImageButtonWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state & ~HOVERED);

    base_type::hoverLeaveEvent(event);
}

void QnImageButtonWidget::resizeEvent(QGraphicsSceneResizeEvent *event) {
    invalidatePixmapCache();

    base_type::resizeEvent(event);
}

void QnImageButtonWidget::changeEvent(QEvent *event) {
    switch(event->type()) {
    case QEvent::StyleChange:
        setFocusPolicy(Qt::FocusPolicy(style()->styleHint(QStyle::SH_Button_FocusPolicy)));
        break;
    default:
        break;
    }

    base_type::changeEvent(event);
}

bool QnImageButtonWidget::event(QEvent *event) {
    QActionEvent *actionEvent = static_cast<QActionEvent *>(event);

    switch (event->type()) {
    case QEvent::ActionChanged:
        if (actionEvent->action() == m_action)
            setDefaultAction(actionEvent->action()); /** Update button state. */
        break;
    case QEvent::ActionAdded:
        break;
    case QEvent::ActionRemoved:
        if (actionEvent->action() == m_action)
            m_action = NULL;
        break;
    default:
        break;
    }

    return base_type::event(event);
}

QVariant QnImageButtonWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    switch(change) {
    case ItemSceneHasChanged:
        m_animator->setTimer(InstrumentManager::animationTimerOf(scene()));
        break;
    case ItemEnabledHasChanged:
        updateState(isDisabled() ? (m_state | DISABLED) : (m_state & ~DISABLED));
        break;
    default:
        break;
    }

    return base_type::itemChange(change, value);
}

QnImageButtonWidget::StateFlags QnImageButtonWidget::displayState(StateFlags flags) const {
    /* Some compilers don't allow expressions in case labels, so we have to
     * precalculate them. */
    enum {
        CHECKED = QnImageButtonWidget::CHECKED,
        HOVERED = QnImageButtonWidget::HOVERED,
        DISABLED = QnImageButtonWidget::DISABLED,
        PRESSED = QnImageButtonWidget::PRESSED,
        CHECKED_HOVERED_DISABLED_PRESSED = CHECKED | HOVERED | DISABLED | PRESSED,
        CHECKED_HOVERED_DISABLED = CHECKED | HOVERED | DISABLED,
        CHECKED_HOVERED = CHECKED | HOVERED,
        CHECKED_DISABLED = CHECKED | DISABLED,
        HOVERED_DISABLED = HOVERED | DISABLED,
        CHECKED_HOVERED_PRESSED = CHECKED | HOVERED | PRESSED,
        CHECKED_DISABLED_PRESSED = CHECKED | DISABLED | PRESSED,
        HOVERED_DISABLED_PRESSED = HOVERED | DISABLED | PRESSED,
        CHECKED_PRESSED = CHECKED | PRESSED,
        HOVERED_PRESSED = HOVERED | PRESSED,
        DISABLED_PRESSED = DISABLED | PRESSED
    };

    switch(flags) {
#define TRY(FLAGS)                                                              \
        if(!m_pixmaps[(FLAGS)].isNull())                                        \
            return static_cast<QnImageButtonWidget::StateFlags>(FLAGS);
    case CHECKED_HOVERED_DISABLED_PRESSED:
        TRY(CHECKED | HOVERED | DISABLED | PRESSED);
        /* Fall through. */
    case CHECKED_HOVERED_DISABLED:
        TRY(CHECKED | HOVERED | DISABLED);
        TRY(CHECKED | DISABLED);
        TRY(CHECKED);
        return 0;
    case CHECKED_HOVERED_PRESSED:
        TRY(CHECKED | HOVERED | PRESSED);
        /* Fall through. */
    case CHECKED_HOVERED:
        TRY(CHECKED | HOVERED);
        TRY(CHECKED);
        return 0;
    case CHECKED_DISABLED_PRESSED:
        TRY(CHECKED | DISABLED | PRESSED);
        /* Fall through. */
    case CHECKED_DISABLED:
        TRY(CHECKED | DISABLED);
        TRY(CHECKED);
        return 0;
    case HOVERED_DISABLED_PRESSED:
        TRY(HOVERED | DISABLED | PRESSED);
        /* Fall through. */
    case HOVERED_DISABLED:
        TRY(HOVERED | DISABLED);
        TRY(DISABLED);
        return 0;
    case CHECKED_PRESSED:
        TRY(CHECKED | PRESSED);
        /* Fall through. */
    case CHECKED:
        TRY(CHECKED);
        return 0;
    case HOVERED_PRESSED:
        TRY(HOVERED | PRESSED);
        /* Fall through. */
    case HOVERED:
        TRY(HOVERED);
        return 0;
    case DISABLED_PRESSED:
        TRY(DISABLED | PRESSED);
        /* Fall through. */
    case DISABLED:
        TRY(DISABLED);
        return 0;
    case PRESSED:
        TRY(PRESSED);
        return 0;
    case 0:
        return 0;
    default:
        return 0;
#undef TRY
    }
}

void QnImageButtonWidget::updateState(StateFlags state) {
    if(m_state == state)
        return;

    StateFlags oldState = m_state;
    m_state = state;

    if((oldState ^ m_state) & CHECKED) { /* CHECKED has changed, emit notification signal and sync with action. */
        emit toggled(isChecked());

        if(m_action != NULL)
            m_action->setChecked(isChecked());
    }

    if((oldState ^ m_state) & DISABLED) /* DISABLED has changed, perform back-sync. */
        setDisabled(m_state & DISABLED);

    if(m_action != NULL && !(oldState & HOVERED) && (m_state & HOVERED)) /* !HOVERED -> HOVERED transition */
        m_action->hover();

    if((oldState & PRESSED) && !(m_state & PRESSED)) /* PRESSED -> !PRESSED */
        m_hoverProgress = 0.0; /* No animation here as it looks crappy. */

    qreal hoverProgress = isHovered() ? 1.0 : 0.0;
    if(scene() == NULL) {
        m_hoverProgress = hoverProgress;
    } else {
        m_animator->animateTo(hoverProgress);
    }
}

void QnImageButtonWidget::setDefaultAction(QAction *action) {
    m_action = action;
    if (!action)
        return;

    if (!this->actions().contains(action))
        addAction(action); /* This way we will receive action-related events and thus will track changes in action state. */

    setIcon(action->icon());
    setToolTip(action->toolTip());

    setCheckable(action->isCheckable());
    setChecked(action->isChecked());
    setEnabled(action->isEnabled());
}

bool QnImageButtonWidget::isCached() const {
    return m_cached;
}

void QnImageButtonWidget::setCached(bool cached) {
    if(m_cached == cached)
        return;

    m_cached = cached;
}

void QnImageButtonWidget::ensurePixmapCache() const {
    if(m_pixmapCacheValid)
        return;

    for(int i = 0; i < FLAGS_MAX; i++) {
        if(m_pixmaps[i].isNull()) {
            m_pixmapCache[i] = m_pixmaps[i];
        } else {
            m_pixmapCache[i] = QPixmap::fromImage(m_pixmaps[i].toImage().scaled(size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }

    m_pixmapCacheValid = true;
}

void QnImageButtonWidget::invalidatePixmapCache() {
    m_pixmapCacheValid = false;
}


// -------------------------------------------------------------------------- //
// QnZoomingImageButtonWidget
// -------------------------------------------------------------------------- //
QnZoomingImageButtonWidget::QnZoomingImageButtonWidget(QGraphicsItem *parent):
    QnImageButtonWidget(parent),
    m_scaleFactor(1.0)
{}

bool QnZoomingImageButtonWidget::isScaledState(StateFlags state) {
    return !(state & HOVERED) || (state & PRESSED);
}

void QnZoomingImageButtonWidget::paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget) {

    qreal startScale = isScaledState(startState) ? m_scaleFactor : 1.0;
    qreal endScale = isScaledState(endState) ? m_scaleFactor : 1.0;
    qreal scale = startScale * progress + endScale * (1.0 - progress);

    if(!qFuzzyCompare(scale, 1.0)) {
        QnScopedPainterTransformRollback guard(painter);
        painter->translate(rect().center());
        painter->scale(scale, scale);
        painter->translate(-rect().center());
        QnImageButtonWidget::paint(painter, startState, endState, progress, widget);
    } else {
        QnImageButtonWidget::paint(painter, startState, endState, progress, widget);
    }
}
