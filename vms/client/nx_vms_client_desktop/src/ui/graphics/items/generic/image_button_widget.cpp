// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "image_button_widget.h"

#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyle>

#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/common/utils/accessor.h>
#include <nx/vms/client/desktop/opengl/opengl_renderer.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/opengl/gl_buffer_stream.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/graphics/shaders/texture_transition_shader_program.h>
#include <ui/workaround/gl_native_painting.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/checked_cast.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/math/linear_combination.h>

#include "image_button_widget_p.h"

//#define QN_IMAGE_BUTTON_WIDGET_DEBUG

using namespace nx::vms::client::desktop;

namespace {
    bool checkPixmapGroupRole(QnImageButtonWidget::StateFlags *flags)
    {
        bool result = true;

        if (*flags < 0 || *flags > QnImageButtonWidget::MaxState)
        {
            NX_ASSERT(false, "Invalid pixmap flags '%1'.", static_cast<int>(*flags));
            *flags = {};
            result = false;
        }

        return result;
    }

    QPixmap bestPixmap(const QIcon &icon, QIcon::Mode mode, QIcon::State state)
    {
        return icon.pixmap(QSize(1024, 1024), mode, state);
    }

    const int VERTEX_POS_INDX = 0;
    const int VERTEX_TEXCOORD0_INDX = 1;

} // anonymous namespace

class QnImageButtonHoverProgressAccessor : public AbstractAccessor
{
    virtual QVariant get(const QObject *object) const override
    {
        return static_cast<const QnImageButtonWidget *>(object)->m_hoverProgress;
    }

    virtual void set(QObject *object, const QVariant &value) const override
    {
        QnImageButtonWidget *widget = static_cast<QnImageButtonWidget *>(object);
        if (qFuzzyEquals(widget->m_hoverProgress, value.toReal()))
            return;

        widget->m_hoverProgress = value.toReal();
        widget->update();
    }
};

QnImageButtonWidget::QnImageButtonWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags)
{
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setClickableButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton));

    m_animator = new VariantAnimator(this);
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new QnImageButtonHoverProgressAccessor());
    m_animator->setSpeed(1000.0);
    registerAnimation(m_animator);

    /* When hovering over a button, a cursor should always change to arrow pointer. */
    setCursor(Qt::ArrowCursor);

    QEvent styleChange(QEvent::StyleChange);
    event(&styleChange);

    connect(this, &GraphicsWidget::enabledChanged, this, [this]()
    {
        if (!isEnabled())
            updateState(m_state & ~Hovered);
    });

    installEventHandler(this, QEvent::UngrabMouse, this,
        [this]()
        {
            if (!isPressed() || !isEnabled())
                return;

            setPressed(false);
            emit released();
        });
}

QnImageButtonWidget::~QnImageButtonWidget()
{
    if (m_glWidget)
        m_glWidget->makeCurrent(); //< Allows to delete textures in appropriate context.
}

const QPixmap &QnImageButtonWidget::pixmap(StateFlags flags) const
{
    return m_pixmaps[validPixmapState(flags)];
}

void QnImageButtonWidget::setPixmap(StateFlags flags, const QPixmap &pixmap)
{
    if (!checkPixmapGroupRole(&flags))
        return;

    m_pixmaps[flags] = pixmap;

    // The same pixmap may be the source for a lot of textures, so clear them all for simplicity.
    // Also wee have to release textures only when the OpenGL context of creation
    // is equal to the current one. So, we store textures to delete them later in the appropriate
    // place.
    m_texturesForRemove.push_back(std::move(m_textures));
    update();
}

QIcon QnImageButtonWidget::icon() const
{
    return m_icon;
}

void QnImageButtonWidget::setIcon(const QIcon &icon)
{
    for (int i = 0; i <= MaxState; i++)
        m_pixmaps[i] = QPixmap();

    setPixmap(Default, bestPixmap(icon, QnIcon::Normal, QnIcon::Off));
    setPixmap(Hovered, bestPixmap(icon, QnIcon::Active, QnIcon::Off));
    setPixmap(Disabled, bestPixmap(icon, QnIcon::Disabled, QnIcon::Off));
    setPixmap(Pressed, bestPixmap(icon, QnIcon::Pressed, QnIcon::Off));

    setPixmap(Checked, bestPixmap(icon, QnIcon::Normal, QnIcon::On));
    setPixmap(Checked | Hovered, bestPixmap(icon, QnIcon::Active, QnIcon::On));
    setPixmap(Checked | Disabled, bestPixmap(icon, QnIcon::Disabled, QnIcon::On));
    setPixmap(Checked | Pressed, bestPixmap(icon, QnIcon::Pressed, QnIcon::On));

    m_actionIconOverridden = true;

    m_icon = icon;
    emit iconChanged();
}

void QnImageButtonWidget::setCheckable(bool checkable)
{
    if (m_checkable == checkable)
        return;

    m_checkable = checkable;
    if (!m_checkable)
        updateState(m_state & ~Checked);
    update();
}

void QnImageButtonWidget::setChecked(bool checked)
{
    if (!m_checkable || checked == isChecked())
        return;

    updateState(checked ? (m_state | Checked) : (m_state & ~Checked));
    update();
}

void QnImageButtonWidget::setDisabled(bool disabled)
{
    setEnabled(!disabled);
}

void QnImageButtonWidget::setPressed(bool pressed)
{
    if (pressed)
    {
        updateState(m_state | Pressed);
    }
    else
    {
        updateState(m_state & ~Pressed);
    }
}

qreal QnImageButtonWidget::animationSpeed() const
{
    return m_animator->speed();
}

void QnImageButtonWidget::setAnimationSpeed(qreal animationSpeed)
{
    m_animator->setSpeed(animationSpeed);
}

void QnImageButtonWidget::clickInternal(QGraphicsSceneMouseEvent *event)
{
    if (isDisabled())
        return;

    QPointer<QObject> self(this);

    QScopedValueRollback<bool> clickedRollback(m_isClicked, true);

    if (m_action != nullptr)
    {
        m_action->trigger();
    }
    else
    {
        toggle();
    }
    if (!self)
        return;

    emit clicked(isChecked());
    if (!self)
        return;

    if (m_action && m_action->menu())
    {
        QGraphicsView *view = nullptr;
        if (event)
            view = qobject_cast<QGraphicsView *>(event->widget() ? event->widget()->parent() : nullptr);
        if (!view && scene() && !scene()->views().empty())
            view = scene()->views()[0];
        if (!view)
        {
            NX_ASSERT(false, "No graphics view in scope, cannot show menu for an image button widget.");
            return;
        }

        QMenu *menu = m_action->menu();
        QPoint pos = view->mapToGlobal(view->mapFromScene(mapToScene(rect().bottomLeft())));

        updateState(m_state | Pressed);
        menu->exec(pos);
        updateState(m_state & ~Pressed);
    }
}

void QnImageButtonWidget::click()
{
    if (!m_state.testFlag(Pressed))
        clickInternal(nullptr);
}

void QnImageButtonWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget)
{
    paintSharp(painter, [this, widget](QPainter* painter)
        {
            StateFlags hoverState = m_state | Hovered;
            StateFlags normalState = m_state & ~Hovered;
            paint(painter, normalState, hoverState, m_hoverProgress,
                qobject_cast<QOpenGLWidget *>(widget), rect());
        });
}

bool QnImageButtonWidget::safeBindTexture(StateFlags flags)
{
    const auto it = m_textures.find(flags);
    if (it != m_textures.end())
    {
        (*it)->bind();
        return true;
    }

    const auto& texturePixmap = pixmap(flags);
    if (texturePixmap.isNull())
        return false;

    const auto texture = TexturePtr(new QOpenGLTexture(texturePixmap.toImage()));
    m_textures.insert(flags, texture);
    texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    texture->bind();
    return true;
}

void QnImageButtonWidget::paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QOpenGLWidget *glWidget, const QRectF &rect)
{
    QRectF imageRect(rect);
    if (!m_imageMargins.isNull())
        imageRect = nx::vms::client::core::Geometry::eroded(imageRect, m_imageMargins);

    if (!glWidget)
    {
        const auto& texturePixmap = pixmap(m_state);
        if (!texturePixmap.isNull())
            painter->drawPixmap(imageRect.toRect(), texturePixmap);
        return;
    }

    if (!m_initialized)
        initializeVao(imageRect, glWidget);
    else if (m_dynamic)
        updateVao(imageRect);

    const bool isZero = qFuzzyIsNull(progress);
    const bool isOne = qFuzzyCompare(progress, 1.0);

    QnGlNativePainting::begin(glWidget, painter);

    QVector4D shaderColor = QVector4D(1.0, 1.0, 1.0, painter->opacity());

    auto renderer = QnOpenGLRendererManager::instance(glWidget);

    glWidget->makeCurrent();

    if (m_glWidget)
        NX_ASSERT(m_glWidget == glWidget, "Button can't be painted in the different GL widgets!");
    else
        m_glWidget = glWidget;

    const auto functions = glWidget->context()->functions();
    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_texturesForRemove.clear();

    if (isOne || isZero)
    {
        if (!safeBindTexture(isZero ? startState : endState))
            return;

        auto shader = renderer->getTextureShader();
        shader->bind();
        shader->setColor(shaderColor);
        shader->setTexture(0);
        renderer->drawBindedTextureOnQuadVao(&m_verticesStatic, shader);
        shader->release();
    }
    else
    {
        auto shader = renderer->getTextureTransitionShader();

        renderer->glActiveTexture(GL_TEXTURE1);
        if (!safeBindTexture(endState))
            return;

        renderer->glActiveTexture(GL_TEXTURE0);
        if (!safeBindTexture(startState))
            return;

        shader->bind();
        shader->setProgress(progress);
        shader->setTexture1(1);
        shader->setTexture(0);
        shader->setColor(QVector4D(1.0, 1.0, 1.0, painter->opacity()));
        renderer->drawBindedTextureOnQuadVao(&m_verticesTransition, shader);
        shader->release();
    }

    functions->glDisable(GL_BLEND);
    QnGlNativePainting::end(painter);
}

void QnImageButtonWidget::clickedNotify(QGraphicsSceneMouseEvent *event)
{
    clickInternal(event);
}

void QnImageButtonWidget::pressedNotify(QGraphicsSceneMouseEvent *)
{
    setPressed(true);

    emit pressed();
}

void QnImageButtonWidget::releasedNotify(QGraphicsSceneMouseEvent *event)
{
    setPressed(false);

    /* Next hover events that we will receive are enter and move converted from
    * release event, skip them. */
    m_skipNextHoverEvents = 2;
    m_nextHoverEventPos = event->screenPos();

    emit released();
}

bool QnImageButtonWidget::skipHoverEvent(QGraphicsSceneHoverEvent *event)
{
    if (m_skipNextHoverEvents)
    {
        if (event->screenPos() == m_nextHoverEventPos)
        {
            m_skipNextHoverEvents--;
            return true;
        }
        else
        {
            m_skipNextHoverEvents = 0;
            return false;
        }
    }
    else
    {
        return false;
    }
}

void QnImageButtonWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept(); /* Buttons are opaque to hover events. */

    if (skipHoverEvent(event))
        return;

    updateState(m_state | Hovered);

    base_type::hoverEnterEvent(event);
}

void QnImageButtonWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept(); /* Buttons are opaque to hover events. */

    if (skipHoverEvent(event))
        return;

    updateState(m_state | Hovered); /* In case we didn't receive the hover enter event. */

    base_type::hoverMoveEvent(event);
}

void QnImageButtonWidget::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();

    if (rect().contains(event->pos()))
    {
        updateState(m_state | Hovered);
    }
    else
    {
        updateState(m_state & ~Hovered);
    }

    base_type::mouseMoveEvent(event);
}

void QnImageButtonWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    event->accept(); /* Buttons are opaque to hover events. */

    updateState(m_state & ~Hovered);

    base_type::hoverLeaveEvent(event);
}

void QnImageButtonWidget::resizeEvent(QGraphicsSceneResizeEvent *event)
{
    base_type::resizeEvent(event);
}

void QnImageButtonWidget::changeEvent(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::StyleChange:
        setFocusPolicy(Qt::FocusPolicy(style()->styleHint(QStyle::SH_Button_FocusPolicy)));
        break;
    default:
        break;
    }

    base_type::changeEvent(event);
}

void QnImageButtonWidget::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    event->accept(); /* Prevent surprising click-through scenarios. */
    return;
}

bool QnImageButtonWidget::event(QEvent *event)
{
    QActionEvent *actionEvent = static_cast<QActionEvent *>(event);

    switch (event->type())
    {
        /* We process hover events here because they don't get forwarded to event
        * handlers for graphics widgets without decorations. */
    case QEvent::GraphicsSceneHoverEnter:
        if (!acceptHoverEvents())
            return true;
        hoverEnterEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return event->isAccepted();
    case QEvent::GraphicsSceneHoverMove:
        if (!acceptHoverEvents())
            return true;
        hoverMoveEvent(static_cast<QGraphicsSceneHoverEvent *>(event));
        return event->isAccepted();
    case QEvent::GraphicsSceneHoverLeave:
        if (!acceptHoverEvents())
            return true;
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
            m_action = nullptr;
        break;
    default:
        break;
    }

    return base_type::event(event);
}

QVariant QnImageButtonWidget::itemChange(GraphicsItemChange change, const QVariant &value)
{
    switch (change)
    {
    case ItemEnabledHasChanged:
        updateState(isDisabled() ? (m_state | Disabled) : (m_state & ~Disabled));
        break;
    default:
        break;
    }

    return base_type::itemChange(change, value);
}

void QnImageButtonWidget::updateState(StateFlags state)
{
    if (m_state == state)
        return;

    StateFlags oldState = m_state;
    m_state = state;

    if ((oldState ^ m_state) & Checked)
    { /* Checked has changed, emit notification signal and sync with action. */
        emit toggled(isChecked());

        if (m_action != nullptr)
            m_action->setChecked(isChecked());
    }

    if ((oldState ^ m_state) & Disabled)
    { /* Disabled has changed, perform back-sync. */
      /* Disabled state change may have been propagated from parent item,
      * and in this case we shouldn't do any back-sync. */
        bool newDisabled = m_state & Disabled;
        if (newDisabled != isDisabled())
            setDisabled(newDisabled);
    }

    if (m_action != nullptr && !(oldState & Hovered) && (m_state & Hovered)) /* !Hovered -> Hovered transition */
        m_action->hover();

    if ((oldState & Pressed) && !(m_state & Pressed)) /* Pressed -> !Pressed */
        m_hoverProgress = 0.0; /* No animation here as it looks crappy. */

    qreal hoverProgress = isHovered() ? 1.0 : 0.0;
    if (scene() == nullptr || (m_state & Pressed))
    {
        m_animator->stop();
        m_hoverProgress = hoverProgress;
    }
    else
    {
        m_animator->animateTo(hoverProgress);
    }

    emit stateChanged();
}

QAction *QnImageButtonWidget::defaultAction() const
{
    return m_action;
}

void QnImageButtonWidget::setDefaultAction(QAction *action)
{
    m_action = action;
    if (!action)
        return;

    if (!this->actions().contains(action))
        addAction(action); /* This way we will receive action-related events and thus will track changes in action state. */

    m_actionIconOverridden = false;
    updateFromDefaultAction();
}

void QnImageButtonWidget::updateFromDefaultAction()
{
    if (!m_actionIconOverridden)
    {
        setIcon(m_action->icon());
        m_actionIconOverridden = false;
    }

    setVisible(m_action->isVisible());
    setToolTip(m_action->toolTip());
    setCheckable(m_action->isCheckable());
    setChecked(m_action->isChecked());
    setEnabled(m_action->isEnabled());
}

bool QnImageButtonWidget::isDynamic() const
{
    return m_dynamic;
}

void QnImageButtonWidget::setDynamic(bool value)
{
    if (m_dynamic == value)
        return;
    NX_ASSERT(!m_initialized);   //This parameter should be set once before the first painting
    m_dynamic = value;
}

void QnImageButtonWidget::setFixedSize(qreal size)
{
    setFixedSize(QSizeF(size, size));
}

void QnImageButtonWidget::setFixedSize(qreal width, qreal height)
{
    setFixedSize(QSizeF(width, height));
}

void QnImageButtonWidget::setFixedSize(const QSizeF &size)
{
    setMinimumSize(size);
    setMaximumSize(size);
}

QMarginsF QnImageButtonWidget::imageMargins() const
{
    return m_imageMargins;
}

void QnImageButtonWidget::setImageMargins(const QMarginsF& margins)
{
    if (m_imageMargins == margins)
        return;

    m_imageMargins = margins;
    update();
}

QnImageButtonWidget::StateFlags QnImageButtonWidget::validPixmapState(StateFlags flags) const
{
    return findValidState(flags, m_pixmaps, [](const QPixmap &pixmap) { return !pixmap.isNull(); });
}

void QnImageButtonWidget::initializeVao(const QRectF &rect, QOpenGLWidget* glWidget)
{

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

    for (int i = 0; i < 8; i++)
    {
        vertexStream << vertices[i];
        texStream << texCoords[i];
    }

    const int VERTEX_POS_SIZE = 2; // x, y
    const int VERTEX_TEXCOORD0_SIZE = 2; // s and t

    const auto renderer = QnOpenGLRendererManager::instance(glWidget);
                                         /* Init static VAO */
    {
        auto shader = renderer->getTextureShader();
        m_verticesStatic.create();
        m_verticesStatic.bind();

        m_positionBufferStatic.create();
        m_positionBufferStatic.setUsagePattern(m_dynamic ? QOpenGLBuffer::DynamicDraw : QOpenGLBuffer::StaticDraw);
        m_positionBufferStatic.bind();
        m_positionBufferStatic.allocate(data.data(), data.size());
        shader->enableAttributeArray(VERTEX_POS_INDX);
        shader->setAttributeBuffer(VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE);

        m_textureBufferStatic.create();
        m_textureBufferStatic.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_textureBufferStatic.bind();
        m_textureBufferStatic.allocate(texData.data(), texData.size());
        shader->enableAttributeArray(VERTEX_TEXCOORD0_INDX);
        shader->setAttributeBuffer(VERTEX_TEXCOORD0_INDX, GL_FLOAT, 0, VERTEX_TEXCOORD0_SIZE);

        if (!shader->initialized())
        {
            shader->bindAttributeLocation("aPosition", VERTEX_POS_INDX);
            shader->bindAttributeLocation("aTexCoord", VERTEX_TEXCOORD0_INDX);
            shader->markInitialized();
        };

        m_textureBufferStatic.release();
        m_positionBufferStatic.release();
        m_verticesStatic.release();
    }

    /* Init transition VAO */
    {
        auto shader = renderer->getTextureTransitionShader();
        m_verticesTransition.create();
        m_verticesTransition.bind();

        m_positionBufferTransition.create();
        m_positionBufferTransition.setUsagePattern(m_dynamic ? QOpenGLBuffer::DynamicDraw : QOpenGLBuffer::StaticDraw);
        m_positionBufferTransition.bind();
        m_positionBufferTransition.allocate(data.data(), data.size());
        shader->enableAttributeArray(VERTEX_POS_INDX);
        shader->setAttributeBuffer(VERTEX_POS_INDX, GL_FLOAT, 0, VERTEX_POS_SIZE);

        m_textureBufferTransition.create();
        m_textureBufferTransition.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_textureBufferTransition.bind();
        m_textureBufferTransition.allocate(texData.data(), texData.size());
        shader->enableAttributeArray(VERTEX_TEXCOORD0_INDX);
        shader->setAttributeBuffer(VERTEX_TEXCOORD0_INDX, GL_FLOAT, 0, VERTEX_TEXCOORD0_SIZE);

        if (!shader->initialized())
        {
            shader->bindAttributeLocation("aPosition", VERTEX_POS_INDX);
            shader->bindAttributeLocation("aTexCoord", VERTEX_TEXCOORD0_INDX);
            shader->markInitialized();
        };

        m_textureBufferTransition.release();
        m_positionBufferTransition.release();
        m_verticesTransition.release();
    }

    m_initialized = true;
}

void QnImageButtonWidget::updateVao(const QRectF &rect)
{

    GLfloat vertices[] = {
        (GLfloat)rect.left(),   (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.top(),
        (GLfloat)rect.right(),  (GLfloat)rect.bottom(),
        (GLfloat)rect.left(),   (GLfloat)rect.bottom()
    };

    QByteArray data;
    QnGlBufferStream<GLfloat> vertexStream(&data);

    for (int i = 0; i < 8; i++)
        vertexStream << vertices[i];

    m_positionBufferStatic.bind();
    m_positionBufferStatic.write(0, data.data(), data.size());
    m_positionBufferStatic.release();

    m_positionBufferTransition.bind();
    m_positionBufferTransition.write(0, data.data(), data.size());
    m_positionBufferTransition.release();
}
