#include "graphics_qml_view.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsSceneHoverEvent>
#include <QtWidgets/QOpenGLWidget>

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickItem>

#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOpenGLVertexArrayObject>
#include <QtGui/QOpenGLBuffer>

#include <QtCore/QTimer>
#include <QtCore/QtMath>

#include <client_core/client_core_module.h>
#include <ui/workaround/gl_native_painting.h>
#include <opengl_renderer.h>
#include <ui/graphics/shaders/texture_color_shader_program.h>

namespace {

const auto kResizeTimeout = std::chrono::milliseconds(200);
constexpr auto kQuadVertexCount = 4;
constexpr auto kCoordPerVertex = 2; //< x, y
constexpr auto kQuadArrayCount = kQuadVertexCount * kCoordPerVertex;

QWidget* findWindowWidgetOf(QWidget* widget)
{
    if (!widget)
        return nullptr;

    QWidget* window = widget->window();
    if (window)
        return window;

    for (QObject* p = widget->parent(); p; p = p->parent())
    {
        QWidget* widget = dynamic_cast<QWidget*>(p);

        if (!widget)
            continue;

        window = widget->window();
        if (window)
            return window;
    }

    return nullptr;
}

Qt::WindowState resolveWindowState(Qt::WindowStates states)
{
    // No more than one of these 3 can be set.
    if (states.testFlag(Qt::WindowMinimized))
        return Qt::WindowMinimized;
    if (states.testFlag(Qt::WindowMaximized))
        return Qt::WindowMaximized;
    if (states.testFlag(Qt::WindowFullScreen))
        return Qt::WindowFullScreen;

    // No state means "windowed" - we ignore Qt::WindowActive.
    return Qt::WindowNoState;
}

void remapInputMethodQueryEvent(QObject* object, QInputMethodQueryEvent* e)
{
    auto item = qobject_cast<QQuickItem*>(object);
    if (!item)
        return;

    // Remap all QRectF values.
    for (auto query: {Qt::ImCursorRectangle, Qt::ImAnchorRectangle, Qt::ImInputItemClipRectangle})
    {
        if (e->queries() & query)
        {
            auto value = e->value(query);
            if (value.canConvert<QRectF>())
                e->setValue(query, item->mapRectToScene(value.toRectF()));
        }
    }
    // Remap all QPointF values.
    if (e->queries() & Qt::ImCursorPosition)
    {
        auto value = e->value(Qt::ImCursorPosition);
        if (value.canConvert<QPointF>())
            e->setValue(Qt::ImCursorPosition, item->mapToScene(value.toPointF()));
    }
}

void initializeQuadBuffer(
    QOpenGLBuffer* buffer,
    QnTextureGLShaderProgram* shader,
    const int location,
    const QOpenGLBuffer::UsagePattern pattern,
    const GLfloat* values)
{
    buffer->create();
    buffer->setUsagePattern(pattern);

    buffer->bind();
    buffer->allocate(values, kQuadArrayCount * sizeof(GLfloat));

    shader->enableAttributeArray(location);
    shader->setAttributeBuffer(location, GL_FLOAT, 0, kCoordPerVertex);
    buffer->release();
}

} // namespace

namespace nx::vms::client::desktop {

class RenderControl: public QQuickRenderControl
{
    using base_type = QQuickRenderControl;

public:
    RenderControl(GraphicsQmlView* parent):
        QQuickRenderControl(parent),
        m_graphicsQmlView(parent)
    {}

    virtual ~RenderControl() override;

    virtual QWindow* renderWindow(QPoint* offset) override;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        switch (event->type())
        {
            case QEvent::Show:
            case QEvent::Move:
            {
                // Maintain screen position for offscreen window by observing
                // onscreen window position changes.
                QPoint offset;
                if (QWindow* w = renderWindow(&offset))
                {
                    const QPoint pos = w->mapToGlobal(offset);
                    m_graphicsQmlView->quickWindow()->setPosition(pos);
                }
                break;
            }
            case QEvent::WindowStateChange:
                m_graphicsQmlView->quickWindow()->setWindowState(resolveWindowState(m_monitoredWidget->windowState()));
                break;
            default:
                break;
        }
        return base_type::eventFilter(obj, event);
    }

private:
    GraphicsQmlView* m_graphicsQmlView;
    QWidget* m_monitoredWidget = nullptr;
};

RenderControl::~RenderControl()
{
    if (m_monitoredWidget)
        m_monitoredWidget->removeEventFilter(this);
}

QWindow* RenderControl::renderWindow(QPoint* offset)
{
    if (offset)
        *offset = QPoint(0, 0);

    QGraphicsScene* scene = m_graphicsQmlView->scene();
    if (!scene)
        return nullptr;

    QList<QGraphicsView*> views = m_graphicsQmlView->scene()->views();
    if (views.isEmpty())
        return nullptr;

    QGraphicsView* view = views.first();
    QWidget* viewport = view->viewport();
    QWidget* windowWidget = findWindowWidgetOf(view->viewport());
    QWindow* window = windowWidget->windowHandle();

    if (windowWidget != m_monitoredWidget)
    {
        if (m_monitoredWidget)
            m_monitoredWidget->removeEventFilter(this);

        m_monitoredWidget = windowWidget;
        m_monitoredWidget->installEventFilter(this);
    }

    if (offset && window)
    {
        QPointF scenePoint = m_graphicsQmlView->mapToScene(QPointF(0.0, 0.0));
        QPoint viewPoint = view->mapFromScene(scenePoint);
        *offset = viewport->mapTo(windowWidget, viewPoint);
    }

    return window;
}

struct GraphicsQmlView::Private
{
    GraphicsQmlView* m_view;
    bool m_initialized = false;
    QScopedPointer<QQuickWindow> m_quickWindow;
    QScopedPointer<QQuickItem> m_rootItem;
    QScopedPointer<RenderControl> m_renderControl;
    QScopedPointer<QOffscreenSurface> m_offscreenSurface;
    QScopedPointer<QOpenGLFramebufferObject> m_fbo;
    QScopedPointer<QQmlComponent> m_qmlComponent;
    QQmlEngine* m_qmlEngine;
    QTimer* m_resizeTimer;

    QOpenGLVertexArrayObject m_vertices;
    QOpenGLBuffer m_positionBuffer;
    QOpenGLBuffer m_texcoordBuffer;
    bool m_vaoInitialized = false;

    Private(GraphicsQmlView* view): m_view(view) {}

    void initialize(const QRectF& rect, QOpenGLWidget* glWidget);
    void ensureVao(QnTextureGLShaderProgram* shader);
    void updateSizes();
    void ensureFbo();
    void scheduleUpdateSizes();
};

GraphicsQmlView::GraphicsQmlView(QGraphicsItem* parent, Qt::WindowFlags wFlags):
    QGraphicsWidget(parent, wFlags), d(new Private(this))
{
    d->m_renderControl.reset(new RenderControl(this));
    connect(d->m_renderControl.data(), &QQuickRenderControl::renderRequested, this,
        [this]()
        {
            this->update();
        }
    );

    connect(d->m_renderControl.data(), &QQuickRenderControl::sceneChanged, this,
        [this]()
        {
            this->update();
        }
    );

    d->m_quickWindow.reset(new QQuickWindow(d->m_renderControl.data()));
    d->m_quickWindow->setTitle(QString::fromLatin1("Offscreen"));
    d->m_quickWindow->setGeometry(0, 0, 640, 480); //< Will be resized later.

    connect(d->m_quickWindow.data(), &QQuickWindow::sceneGraphInitialized, this,
        [this]()
        {
            d->ensureFbo();
        }
    );

    d->m_resizeTimer = new QTimer(this);
    d->m_resizeTimer->setSingleShot(true);
    d->m_resizeTimer->setInterval(kResizeTimeout);
    connect(d->m_resizeTimer, &QTimer::timeout, this, [this](){ d->updateSizes(); });

    d->m_qmlEngine = qnClientCoreModule->mainQmlEngine();

    d->m_qmlComponent.reset(new QQmlComponent(d->m_qmlEngine));
    connect(d->m_qmlComponent.data(), &QQmlComponent::statusChanged, this,
        [this](QQmlComponent::Status status)
        {
            switch (status)
            {
                case QQmlComponent::Null:
                    emit statusChanged(QQuickWidget::Null);
                    return;
                case QQmlComponent::Ready:
                {
                    QObject* rootObject = d->m_qmlComponent->create();
                    d->m_rootItem.reset(qobject_cast<QQuickItem*>(rootObject));
                    if (!d->m_rootItem)
                        return;

                    d->m_rootItem->setParentItem(d->m_quickWindow->contentItem());

                    d->scheduleUpdateSizes();
                    emit statusChanged(QQuickWidget::Ready);
                    return;
                }
                case QQmlComponent::Loading:
                    emit statusChanged(QQuickWidget::Loading);
                    return;
                case QQmlComponent::Error:
                    for (const auto& error: errors())
                        NX_ERROR(this, "QML Error: %1", error.toString());
                    emit statusChanged(QQuickWidget::Error);
                    return;
            }
        });

    setAcceptHoverEvents(true);
    setFocusPolicy(Qt::StrongFocus);
}

GraphicsQmlView::~GraphicsQmlView()
{
    if (d->m_rootItem)
        d->m_rootItem.reset();
    d->m_qmlComponent.reset();
    d->m_quickWindow.reset();
    d->m_renderControl.reset();
    if (d->m_fbo)
        d->m_fbo.reset();
}

QQuickWindow* GraphicsQmlView::quickWindow() const
{
    return d->m_quickWindow.data();
}

QQuickItem* GraphicsQmlView::rootObject() const
{
    return d->m_rootItem.data();
}

QList<QQmlError> GraphicsQmlView::errors() const
{
    return d->m_qmlComponent->errors();
}

void GraphicsQmlView::Private::scheduleUpdateSizes()
{
    m_resizeTimer->start();
}

void GraphicsQmlView::updateWindowGeometry()
{
    d->scheduleUpdateSizes();
}

QQmlEngine* GraphicsQmlView::engine() const
{
    return d->m_qmlEngine;
}

void GraphicsQmlView::Private::ensureFbo()
{
    if (!m_quickWindow)
        return;

    QSize fboSize = m_quickWindow->size() * qApp->devicePixelRatio();

    if (m_fbo && m_fbo->size() != fboSize)
        m_fbo.reset();

    if (!m_fbo)
    {
        m_fbo.reset(new QOpenGLFramebufferObject(fboSize, QOpenGLFramebufferObject::CombinedDepthStencil));
        m_quickWindow->setRenderTarget(m_fbo.data());
    }
}

bool GraphicsQmlView::event(QEvent* event)
{
    // From https://github.com/qt/qtdeclarative/blob/dev/src/quickwidgets/qquickwidget.cpp
    switch (event->type())
    {
        case QEvent::Leave:
        case QEvent::TouchBegin:
        case QEvent::TouchEnd:
        case QEvent::TouchUpdate:
        case QEvent::TouchCancel:
            // Touch events only have local and global positions, no need to map.
            return QCoreApplication::sendEvent(d->m_quickWindow.data(), event);
        case QEvent::InputMethod:
            return QCoreApplication::sendEvent(d->m_quickWindow.data(), event);
        case QEvent::InputMethodQuery:
        {
            bool eventResult = QCoreApplication::sendEvent(d->m_quickWindow->focusObject(), event);
            // The result in focusObject are based on m_quickWindow. But
            // the inputMethodTransform won't get updated because the focus
            // is on GraphicsQmlView. We need to remap the value based on the
            // widget.
            remapInputMethodQueryEvent(d->m_quickWindow->focusObject(), static_cast<QInputMethodQueryEvent*>(event));
            return eventResult;
        }
        case QEvent::GraphicsSceneResize:
        case QEvent::GraphicsSceneMove:
            d->scheduleUpdateSizes();
            event->accept();
            break;
        case QEvent::ShortcutOverride:
            return QCoreApplication::sendEvent(d->m_quickWindow.data(), event);
        default:
            break;
    }

    return base_type::event(event);
}

void GraphicsQmlView::setSource(const QUrl& url)
{
    d->m_qmlComponent->loadUrl(url);
}

void GraphicsQmlView::setData(const QByteArray& data, const QUrl& url)
{
    d->m_qmlComponent->setData(data, url);
}

void GraphicsQmlView::Private::updateSizes()
{
    if (!m_rootItem || !m_quickWindow)
        return;

    const auto size = m_view->size();
    m_rootItem->setSize(size);

    QPoint offset;
    QWindow* w = m_renderControl->renderWindow(&offset);
    const QPoint pos = w ? w->mapToGlobal(offset) : offset;

    m_quickWindow->setGeometry(pos.x(), pos.y(), qCeil(size.width()), qCeil(size.height()));
}

void GraphicsQmlView::Private::initialize(const QRectF&, QOpenGLWidget* glWidget)
{
    auto context = glWidget->context();

    m_offscreenSurface.reset(new QOffscreenSurface());
    QSurfaceFormat format = context->format();
    // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(8);

    m_offscreenSurface->setFormat(format);
    m_offscreenSurface->setScreen(context->screen());
    m_offscreenSurface->create();

    context->makeCurrent(m_offscreenSurface.data());
    m_renderControl->initialize(context);
    context->doneCurrent();

    m_initialized = true;
}

void GraphicsQmlView::Private::ensureVao(QnTextureGLShaderProgram* shader)
{
    if (m_vaoInitialized)
        return;

    static constexpr GLfloat coordArray[kQuadArrayCount] = {
        0.0, 0.0,
        1.0, 0.0,
        1.0, 1.0,
        0.0, 1.0
    };

    m_vertices.create();
    m_vertices.bind();

    const auto VERTEX_POS_LOCATION = 0;
    initializeQuadBuffer(
        &m_positionBuffer,
        shader,
        VERTEX_POS_LOCATION,
        QOpenGLBuffer::DynamicDraw,
        coordArray);

    const auto VERTEX_TEXCOORD0_LOCATION = 1;
    initializeQuadBuffer(
        &m_texcoordBuffer,
        shader,
        VERTEX_TEXCOORD0_LOCATION,
        QOpenGLBuffer::StaticDraw,
        coordArray);

    shader->bindAttributeLocation("aPosition", VERTEX_POS_LOCATION);
    shader->bindAttributeLocation("aTexcoord", VERTEX_TEXCOORD0_LOCATION);
    shader->markInitialized();

    m_vertices.release();

    m_vaoInitialized = true;
}

void GraphicsQmlView::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseMove, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->m_quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseButtonPress, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->m_quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();
    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseButtonRelease, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->m_quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseMove, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->m_quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    const QPointF mousePoint = event->pos();
    const QPoint globalPos = event->screenPos();
    QWheelEvent mappedEvent(mousePoint, globalPos, event->delta(), event->buttons(), event->modifiers(), event->orientation());
    QCoreApplication::sendEvent(d->m_quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::keyPressEvent(QKeyEvent* event)
{
    QCoreApplication::sendEvent(d->m_quickWindow.data(), event);
}

void GraphicsQmlView::keyReleaseEvent(QKeyEvent* event)
{
    QCoreApplication::sendEvent(d->m_quickWindow.data(), event);
}

void GraphicsQmlView::focusInEvent(QFocusEvent* event)
{
    QCoreApplication::sendEvent(d->m_quickWindow.data(), event);
}

void GraphicsQmlView::focusOutEvent(QFocusEvent* event)
{
    QCoreApplication::sendEvent(d->m_quickWindow.data(), event);
}

bool GraphicsQmlView::focusNextPrevChild(bool next)
{
    QKeyEvent event(QEvent::KeyPress, next ? Qt::Key_Tab : Qt::Key_Backtab, Qt::NoModifier);
    QCoreApplication::sendEvent(d->m_quickWindow.data(), &event);

    QKeyEvent releaseEvent(QEvent::KeyRelease, next ? Qt::Key_Tab : Qt::Key_Backtab, Qt::NoModifier);
    QCoreApplication::sendEvent(d->m_quickWindow.data(), &releaseEvent);

    return event.isAccepted();
}

void GraphicsQmlView::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!d->m_rootItem)
        return;

    const auto glWidget = qobject_cast<QOpenGLWidget*>(scene()->views().first()->viewport());

    auto channelRect = rect();

    QnGlNativePainting::begin(glWidget, painter);

    if (!d->m_initialized)
        d->initialize(channelRect, glWidget);

    d->ensureFbo();

    if (d->m_fbo && glWidget->context()->makeCurrent(d->m_offscreenSurface.data()))
    {
        d->m_renderControl->polishItems();
        d->m_renderControl->sync();

        d->m_renderControl->render();
        d->m_quickWindow->resetOpenGLState();
        glWidget->context()->functions()->glFlush();
    }

    QnGlNativePainting::end(painter);

    if (!d->m_fbo)
        return;

    QnGlNativePainting::begin(glWidget, painter);

    auto renderer = QnOpenGLRendererManager::instance(glWidget);
    glWidget->makeCurrent();

    const auto functions = glWidget->context()->functions();
    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    functions->glActiveTexture(GL_TEXTURE0);
    functions->glBindTexture(GL_TEXTURE_2D, d->m_fbo->texture());

    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    const GLfloat posArray[kQuadArrayCount] = {
        (float)channelRect.left(), (float)channelRect.bottom(),
        (float)channelRect.right(), (float)channelRect.bottom(),
        (float)channelRect.right(), (float)channelRect.top(),
        (float)channelRect.left(), (float)channelRect.top()
    };

    const auto shaderColor = QVector4D(1.0, 1.0, 1.0, painter->opacity());

    auto shader = renderer->getTextureShader();

    d->ensureVao(shader);
    d->m_positionBuffer.bind();
    d->m_positionBuffer.write(0, posArray, kQuadArrayCount * sizeof(GLfloat));
    d->m_positionBuffer.release();

    shader->bind();
    shader->setColor(shaderColor);
    shader->setTexture(0);
    renderer->drawBindedTextureOnQuadVao(&d->m_vertices, shader);
    shader->release();

    QnGlNativePainting::end(painter);
}

} // namespace nx::vms::client::desktop
