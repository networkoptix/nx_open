#include "image_button_widget.h"
#include "image_button_widget_p.h"

#include <cassert>

#include <QtGui/QPainter>
#include <QtGui/QIcon>
#include <QtWidgets/QAction>
#include <QtWidgets/QStyle>
#include <QtGui/QPixmapCache>
#include <QtOpenGL/QGLContext>

#include <ui/workaround/gl_native_painting.h>
#include <ui/animation/variant_animator.h>
#include <ui/style/globals.h>
#include <ui/style/icon.h>

#include <ui/graphics/shaders/texture_transition_shader_program.h>

#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include <ui/common/geometry.h>
#include <ui/common/accessor.h>
#include <ui/common/palette.h>

#include <utils/common/warnings.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/math/linear_combination.h>
#include <utils/math/color_transformations.h>
#include "opengl_renderer.h"

//#define QN_IMAGE_BUTTON_WIDGET_DEBUG

namespace {
    bool checkPixmapGroupRole(QnImageButtonWidget::StateFlags *flags) {
        bool result = true;

        if(*flags < 0 || *flags > QnImageButtonWidget::MaxState) {
            qnWarning("Invalid pixmap flags '%1'.", static_cast<int>(*flags));
            *flags = 0;
            result = false;
        }

        return result;
    }

    GLuint checkedBindTexture(QGLWidget *widget, const QPixmap &pixmap, GLenum target, GLint format, QGLContext::BindOptions options) {
        GLint result = widget->bindTexture(pixmap, target, format, options);
#ifdef QN_IMAGE_BUTTON_WIDGET_DEBUG
        if(!glIsTexture(result))
            qnWarning("OpenGL texture %1 was unexpectedly released, rendering glitches may ensue.", result);
#endif
        return result;
    }

    QPixmap bestPixmap(const QIcon &icon, QIcon::Mode mode, QIcon::State state) {
        return icon.pixmap(QSize(1024, 1024), mode, state);
    }

    typedef QnGlContextData<QnTextureTransitionShaderProgram, QnGlContextDataForwardingFactory<QnTextureTransitionShaderProgram> > QnTextureTransitionShaderProgramStorage;
    Q_GLOBAL_STATIC(QnTextureTransitionShaderProgramStorage, qn_textureTransitionShaderProgramStorage)

    const int VERTEX_POS_INDX = 0;
    const int VERTEX_TEXCOORD0_INDX = 1;

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

QnImageButtonWidget::QnImageButtonWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_pixmapCacheValid(false),
    m_state(0),
    m_checkable(false),
    m_cached(false),
    m_skipNextHoverEvents(false),
    m_hoverProgress(0.0),
    m_action(NULL),
    m_actionIconOverridden(false),
    m_initialized(false)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton));

    m_animator = new VariantAnimator(this);
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new QnImageButtonHoverProgressAccessor());
    m_animator->setSpeed(1000.0 / qnGlobals->opacityChangePeriod());
    registerAnimation(m_animator);

    /* When hovering over a button, a cursor should always change to arrow pointer. */
    setCursor(Qt::ArrowCursor);

    QEvent styleChange(QEvent::StyleChange);
    event(&styleChange);
}

QnImageButtonWidget::~QnImageButtonWidget() {
    return;
}

const QPixmap &QnImageButtonWidget::pixmap(StateFlags flags) const {
    if(!m_cached) {
        return m_pixmaps[validPixmapState(flags)];
    } else {
        ensurePixmapCache();

        return m_pixmapCache[validPixmapState(flags)];
    }
}

void QnImageButtonWidget::setPixmap(StateFlags flags, const QPixmap &pixmap) {
    if(!checkPixmapGroupRole(&flags))
        return;

    m_pixmaps[flags] = pixmap;

    invalidatePixmapCache();
    update();
}

QIcon QnImageButtonWidget::icon() const {
    static const QIcon emptyIcon;
    return emptyIcon; // TODO: #Elric #ec2: change to anything appropriate
}

void QnImageButtonWidget::setIcon(const QIcon &icon) {
    for(int i = 0; i <= MaxState; i++)
        m_pixmaps[i] = QPixmap();

    setPixmap(0,                            bestPixmap(icon, QnIcon::Normal, QnIcon::Off));
    setPixmap(Hovered,                      bestPixmap(icon, QnIcon::Active, QnIcon::Off));
    setPixmap(Disabled,                     bestPixmap(icon, QnIcon::Disabled, QnIcon::Off));
    setPixmap(Pressed,                      bestPixmap(icon, QnIcon::Pressed, QnIcon::Off));

    setPixmap(Checked,                      bestPixmap(icon, QnIcon::Normal, QnIcon::On));
    setPixmap(Checked | Hovered,            bestPixmap(icon, QnIcon::Active, QnIcon::On));
    setPixmap(Checked | Disabled,           bestPixmap(icon, QnIcon::Disabled, QnIcon::On));
    setPixmap(Checked | Pressed,            bestPixmap(icon, QnIcon::Pressed, QnIcon::On));

    m_actionIconOverridden = true;
}

void QnImageButtonWidget::setCheckable(bool checkable) {
    if (m_checkable == checkable)
        return;

    m_checkable = checkable;
    if (!m_checkable)
        updateState(m_state & ~Checked);
    update();
}

void QnImageButtonWidget::setChecked(bool checked) {
    if (!m_checkable || checked == isChecked())
        return;

    updateState(checked ? (m_state | Checked) : (m_state & ~Checked));
    update();
}

void QnImageButtonWidget::setDisabled(bool disabled) {
    setEnabled(!disabled);
}

void QnImageButtonWidget::setPressed(bool pressed) {
    if(pressed) {
        updateState(m_state | Pressed);
    } else {
        updateState(m_state & ~Pressed);
    }
}

qreal QnImageButtonWidget::animationSpeed() const {
    return m_animator->speed();
}

void QnImageButtonWidget::setAnimationSpeed(qreal animationSpeed) {
    m_animator->setSpeed(animationSpeed);
}

void QnImageButtonWidget::clickInternal(QGraphicsSceneMouseEvent *event) {
    if(isDisabled())
        return;

    QPointer<QObject> self(this);

    if(m_action != NULL) {
        m_action->trigger();
    } else {
        toggle();
    }
    if(!self)
        return;

    emit clicked(isChecked());
    if(!self)
        return;

    if(m_action && m_action->menu() && (!event || !skipMenuEvent(event))) {
        QGraphicsView *view = NULL;
        if(event)
            view = qobject_cast<QGraphicsView *>(event->widget() ? event->widget()->parent() : NULL);
        if(!view && scene() && !scene()->views().empty())
            view = scene()->views()[0];
        if(!view) {
            qnWarning("No graphics view in scope, cannot show menu for an image button widget.");
            return;
        }

        QMenu *menu = m_action->menu();
        QPoint pos = view->mapToGlobal(view->mapFromScene(mapToScene(rect().bottomLeft())));

        updateState(m_state | Pressed);
        menu->exec(pos);
        updateState(m_state & ~Pressed);

        /* Cannot use QMenu::setNoReplayFor, as it will block click events for the whole scene.
         * This is why we resort to nasty hacks with mouse position comparisons. */
        m_skipNextMenuEvents = 1;
        m_nextMenuEventPos = QCursor::pos();
    }
}

void QnImageButtonWidget::click() {
    clickInternal(NULL);
}

void QnImageButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) {
    if(m_shader.isNull()) {
        m_shader = qn_textureTransitionShaderProgramStorage()->get(QGLContext::currentContext());
        initializeOpenGLFunctions();
    }

    StateFlags hoverState = m_state | Hovered;
    StateFlags normalState = m_state & ~Hovered;
    paint(painter, normalState, hoverState, m_hoverProgress, checked_cast<QGLWidget *>(widget), rect());
}

void QnImageButtonWidget::paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) {

    if (!m_initialized) 
        initializeVao(rect);

    bool isZero = qFuzzyIsNull(progress);
    bool isOne = qFuzzyCompare(progress, 1.0);

    const QPixmap &startPixmap = pixmap(startState);
    const QPixmap &endPixmap = pixmap(endState);

    if(isZero && startPixmap.isNull()) {
        return;
    } else if(isOne && endPixmap.isNull()) {
        return;
    } else if(startPixmap.isNull() && endPixmap.isNull()) {
        return;
    }

    QnGlNativePainting::begin(QGLContext::currentContext(),painter);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QVector4D shaderColor = QVector4D(1.0, 1.0, 1.0, painter->opacity());

    if (isOne || isZero) {
        if (isZero) {
            checkedBindTexture(widget, startPixmap, GL_TEXTURE_2D, GL_RGBA, QGLContext::LinearFilteringBindOption);
        } else {
            checkedBindTexture(widget, endPixmap, GL_TEXTURE_2D, GL_RGBA, QGLContext::LinearFilteringBindOption);
        }
        auto shader = QnOpenGLRendererManager::instance(QGLContext::currentContext())->getTextureShader();

        shader->bind();
        shader->setColor(shaderColor);
        shader->setTexture(0);          
        QnOpenGLRendererManager::instance(QGLContext::currentContext())->drawBindedTextureOnQuadVao(&m_verticesStatic, shader);
        shader->release();
    } else {

        glActiveTexture(GL_TEXTURE1);
        checkedBindTexture(widget, endPixmap, GL_TEXTURE_2D, GL_RGBA, QGLContext::LinearFilteringBindOption);
        glActiveTexture(GL_TEXTURE0);
        checkedBindTexture(widget, startPixmap, GL_TEXTURE_2D, GL_RGBA, QGLContext::LinearFilteringBindOption);
        m_shader->bind();
        m_shader->setProgress(progress);
        m_shader->setTexture1(1);
        m_shader->setTexture(0);
        m_shader->setColor(QVector4D(1.0, 1.0, 1.0, painter->opacity()));
        QnOpenGLRendererManager::instance(QGLContext::currentContext())->drawBindedTextureOnQuadVao(&m_verticesTransition, m_shader.data());
        m_shader->release();
    }

    glDisable(GL_BLEND);
    QnGlNativePainting::end(painter);
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *event) {
    clickInternal(event);
}

void QnImageButtonWidget::pressedNotify(QGraphicsSceneMouseEvent *) {
    setPressed(true);

    emit pressed();
}

void QnImageButtonWidget::releasedNotify(QGraphicsSceneMouseEvent *event) {
    setPressed(false);

    /* Next hover events that we will receive are enter and move converted from
     * release event, skip them. */
    m_skipNextHoverEvents = 2;
    m_nextHoverEventPos = event->screenPos();

    emit released();
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

    updateState(m_state | Hovered);

    base_type::hoverEnterEvent(event);
}

void QnImageButtonWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    if(skipHoverEvent(event))
        return;

    updateState(m_state | Hovered); /* In case we didn't receive the hover enter event. */

    base_type::hoverMoveEvent(event);
}

void QnImageButtonWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    event->accept();

    if(rect().contains(event->pos())) {
        updateState(m_state | Hovered);
    } else {
        updateState(m_state & ~Hovered);
    }

    base_type::mouseMoveEvent(event);
}

void QnImageButtonWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state & ~Hovered);

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

void QnImageButtonWidget::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
    return;
}

bool QnImageButtonWidget::event(QEvent *event) {
    QActionEvent *actionEvent = static_cast<QActionEvent *>(event);

    switch (event->type()) {
        /* We process hover events here because they don't get forwarded to event
         * handlers for graphics widgets without decorations. */
    case QEvent::GraphicsSceneHoverEnter:
        hoverEnterEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return event->isAccepted();
    case QEvent::GraphicsSceneHoverMove:
        hoverMoveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return event->isAccepted();
    case QEvent::GraphicsSceneHoverLeave:
        hoverLeaveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return event->isAccepted();
    case QEvent::ActionChanged:
        if (actionEvent->action() == m_action)
            updateFromDefaultAction();
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
    case ItemEnabledHasChanged:
        updateState(isDisabled() ? (m_state | Disabled) : (m_state & ~Disabled));
        break;
    default:
        break;
    }

    return base_type::itemChange(change, value);
}

void QnImageButtonWidget::updateState(StateFlags state) {
    if(m_state == state)
        return;

    StateFlags oldState = m_state;
    m_state = state;

    if((oldState ^ m_state) & Checked) { /* Checked has changed, emit notification signal and sync with action. */
        emit toggled(isChecked());

        if(m_action != NULL)
            m_action->setChecked(isChecked());
    }

    if((oldState ^ m_state) & Disabled) { /* Disabled has changed, perform back-sync. */
        /* Disabled state change may have been propagated from parent item,
         * and in this case we shouldn't do any back-sync. */
        bool newDisabled = m_state & Disabled;
        if(newDisabled != isDisabled())
            setDisabled(newDisabled);
    }

    if(m_action != NULL && !(oldState & Hovered) && (m_state & Hovered)) /* !Hovered -> Hovered transition */
        m_action->hover();

    if((oldState & Pressed) && !(m_state & Pressed)) /* Pressed -> !Pressed */
        m_hoverProgress = 0.0; /* No animation here as it looks crappy. */

    qreal hoverProgress = isHovered() ? 1.0 : 0.0;
    if(scene() == NULL || (m_state & Pressed)) {
        m_animator->stop();
        m_hoverProgress = hoverProgress;
    } else {
        m_animator->animateTo(hoverProgress);
    }

    emit stateChanged();
}

QAction *QnImageButtonWidget::defaultAction() const {
    return m_action;
}

void QnImageButtonWidget::setDefaultAction(QAction *action) {
    m_action = action;
    if (!action)
        return;

    if (!this->actions().contains(action))
        addAction(action); /* This way we will receive action-related events and thus will track changes in action state. */

    m_actionIconOverridden = false;
    updateFromDefaultAction();
}

void QnImageButtonWidget::updateFromDefaultAction() {
    if(!m_actionIconOverridden) {
        setIcon(m_action->icon());
        m_actionIconOverridden = false;
    }

    setToolTip(m_action->toolTip());
    setCheckable(m_action->isCheckable());
    setChecked(m_action->isChecked());
    setEnabled(m_action->isEnabled()); // TODO: #Elric do backsync?
}

bool QnImageButtonWidget::isCached() const {
    return m_cached;
}

void QnImageButtonWidget::setCached(bool cached) {
    if(m_cached == cached)
        return;

    m_cached = cached;

    invalidatePixmapCache();
}

void QnImageButtonWidget::setFixedSize(qreal size) {
    setFixedSize(QSizeF(size, size));
}

void QnImageButtonWidget::setFixedSize(qreal width, qreal height) {
    setFixedSize(QSizeF(width, height));
}

void QnImageButtonWidget::setFixedSize(const QSizeF &size) {
    setMinimumSize(size);
    setMaximumSize(size);
}

void QnImageButtonWidget::ensurePixmapCache() const {
    if(m_pixmapCacheValid)
        return;

    for(int i = 0; i < MaxState; i++) {
        if(m_pixmaps[i].isNull()) {
            m_pixmapCache[i] = m_pixmaps[i];
        } else {
            QSize size = this->size().toSize();
            QString key = lit("ibw_%1_%2x%3").arg(m_pixmaps[i].cacheKey()).arg(size.width()).arg(size.height());
            
            QPixmap pixmap;
            if (!QPixmapCache::find(key, &pixmap)) {
                pixmap = QPixmap::fromImage(m_pixmaps[i].toImage().scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
                QPixmapCache::insert(key, pixmap);
            }

            m_pixmapCache[i] = pixmap;
        }
    }

    m_pixmapCacheValid = true;
}

void QnImageButtonWidget::invalidatePixmapCache() {
    m_pixmapCacheValid = false;
}

QnImageButtonWidget::StateFlags QnImageButtonWidget::validPixmapState(StateFlags flags) const {
    return findValidState(flags, m_pixmaps, [](const QPixmap &pixmap) { return !pixmap.isNull(); });
}

void QnImageButtonWidget::initializeVao(const QRectF &rect) {

    GLfloat vertices[] = {
        (GLfloat)rect.left(),   (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.bottom(),
        (GLfloat)rect.left(),   (GLfloat)rect.bottom()
    };

    GLfloat texCoords[] = {
        0.0, 0.0,
        1.0, 0.0,
        1.0, 1.0,
        0.0, 1.0
    };

    QByteArray data, texData;
    QnGlBufferStream<GLfloat> vertexStream(&data), texStream(&texData);

    for(int i = 0; i < 8; i++) {
        vertexStream << vertices[i];
        texStream << texCoords[i];
    }

    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_TEXCOORD0_SIZE = 2; // s and t

    /* Init static VAO */
    auto shader = QnOpenGLRendererManager::instance(QGLContext::currentContext())->getTextureShader();
    m_verticesStatic.create();
    m_verticesStatic.bind();

    m_positionBufferStatic.create();
    m_positionBufferStatic.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_positionBufferStatic.bind();
    m_positionBufferStatic.allocate( data.data(), data.size() );
    shader->enableAttributeArray( VERTEX_POS_INDX );
    shader->setAttributeBuffer( VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE );

    m_textureBufferStatic.create();
    m_textureBufferStatic.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_textureBufferStatic.bind();
    m_textureBufferStatic.allocate( texData.data(), texData.size());
    shader->enableAttributeArray( VERTEX_TEXCOORD0_INDX );
    shader->setAttributeBuffer( VERTEX_TEXCOORD0_INDX, GL_FLOAT, 0, VERTEX_TEXCOORD0_SIZE );

    if (!shader->initialized()) {
        shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        shader->bindAttributeLocation("aTexcoord",VERTEX_TEXCOORD0_INDX);
        shader->markInitialized();
    };

    m_textureBufferStatic.release();
    m_positionBufferStatic.release();
    m_verticesStatic.release();

    /* Init transition VAO */
    m_verticesTransition.create();
    m_verticesTransition.bind();

    m_positionBufferTransition.create();
    m_positionBufferTransition.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_positionBufferTransition.bind();
    m_positionBufferTransition.allocate( data.data(), data.size() );
    m_shader->enableAttributeArray( VERTEX_POS_INDX );
    m_shader->setAttributeBuffer( VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE );

    m_textureBufferTransition.create();
    m_textureBufferTransition.setUsagePattern( QOpenGLBuffer::StaticDraw );
    m_textureBufferTransition.bind();
    m_textureBufferTransition.allocate( texData.data(), texData.size());
    m_shader->enableAttributeArray( VERTEX_TEXCOORD0_INDX );
    m_shader->setAttributeBuffer( VERTEX_TEXCOORD0_INDX, GL_FLOAT, 0, VERTEX_TEXCOORD0_SIZE );

    if (!m_shader->initialized()) {
        m_shader->bindAttributeLocation("aPosition",VERTEX_POS_INDX);
        m_shader->bindAttributeLocation("aTexcoord",VERTEX_TEXCOORD0_INDX);
        m_shader->markInitialized();
    };    

    m_textureBufferTransition.release();
    m_positionBufferTransition.release();
    m_verticesTransition.release();



    m_initialized = true;
}


// -------------------------------------------------------------------------- //
// QnRotatingImageButtonWidget
// -------------------------------------------------------------------------- //
QnRotatingImageButtonWidget::QnRotatingImageButtonWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_rotationSpeed(360.0),
    m_rotation(0.0)
{
    registerAnimation(this);
    startListening();
}

void QnRotatingImageButtonWidget::paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget, const QRectF &rect) {
    QnScopedPainterTransformRollback guard(painter);
    painter->translate(rect.center());
    painter->rotate(m_rotation);
    painter->translate(-rect.center());
    QnImageButtonWidget::paint(painter, startState, endState, progress, widget, rect);
}

void QnRotatingImageButtonWidget::tick(int deltaMSecs) {
    if(state() & Checked)
        m_rotation += m_rotationSpeed * deltaMSecs / 1000.0;
}

