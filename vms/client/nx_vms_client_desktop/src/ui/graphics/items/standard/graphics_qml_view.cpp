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

#include <QtCore/QtMath>
#include <QtCore/QElapsedTimer>

#include <client_core/client_core_module.h>
#include <memory>
#include <ui/workaround/gl_native_painting.h>
#include <opengl_renderer.h>
#include <ui/graphics/shaders/texture_color_shader_program.h>

using namespace std::chrono;

namespace {

const auto kFboResizeTimeout = 50ms;
constexpr auto kQuadVertexCount = 4;
constexpr auto kCoordPerVertex = 2; //< x, y
constexpr auto kQuadArrayLength = kQuadVertexCount * kCoordPerVertex;

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
    QnTextureGLShaderProgram* shader,
    const char* attributeName,
    QOpenGLBuffer* buffer,
    const GLfloat* values = nullptr)
{
    buffer->create();
    buffer->setUsagePattern(values ? QOpenGLBuffer::StaticDraw : QOpenGLBuffer::DynamicDraw);

    buffer->bind();
    const auto bufferSize = kQuadArrayLength * sizeof(GLfloat);
    if (values)
        buffer->allocate(values, bufferSize);
    else
        buffer->allocate(bufferSize);

    const auto location = shader->attributeLocation(attributeName);
    NX_ASSERT(location != -1, attributeName);

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
    GraphicsQmlView* view;
    bool offscreenInitialized = false;
    QScopedPointer<QQuickWindow> quickWindow;
    QScopedPointer<QQuickItem> rootItem;
    QScopedPointer<RenderControl> renderControl;
    QScopedPointer<QOffscreenSurface> offscreenSurface;
    QScopedPointer<QOpenGLContext> openglContext;
    QScopedPointer<QOpenGLFramebufferObject> fbo;
    QElapsedTimer fboTimer;
    QScopedPointer<QQmlComponent> qmlComponent;
    QQmlEngine* qmlEngine = nullptr;

    QOpenGLVertexArrayObject vertices;
    QOpenGLBuffer positionBuffer;
    QOpenGLBuffer texcoordBuffer;
    bool vaoInitialized = false;

    bool renderRequested = false;
    bool sceneChanged = false;

    Private(GraphicsQmlView* view): view(view) {}

    void ensureOffscreen(const QRectF& rect, QOpenGLWidget* glWidget);
    void ensureVao(QnTextureGLShaderProgram* shader);
    void updateSizes();
    void tryInitializeFbo();
    void scheduleUpdateSizes();
    void paintQml(QPainter* painter, const QRectF& channelRect, QOpenGLWidget* glWidget);
};

GraphicsQmlView::GraphicsQmlView(QGraphicsItem* parent, Qt::WindowFlags wFlags):
    QGraphicsWidget(parent, wFlags), d(new Private(this))
{
    d->renderControl.reset(new RenderControl(this));

    connect(d->renderControl.data(), &QQuickRenderControl::renderRequested, this,
        [this]()
        {
            d->renderRequested = true;
            this->update();
        }
    );

    connect(d->renderControl.data(), &QQuickRenderControl::sceneChanged, this,
        [this]()
        {
            d->sceneChanged = true;
            this->update();
        }
    );

    d->quickWindow.reset(new QQuickWindow(d->renderControl.data()));
    d->quickWindow->setTitle(QString::fromLatin1("Offscreen"));
    d->quickWindow->setGeometry(0, 0, 640, 480); //< Will be resized later.

    d->qmlEngine = qnClientCoreModule->mainQmlEngine();

    d->qmlComponent.reset(new QQmlComponent(d->qmlEngine));
    connect(d->qmlComponent.data(), &QQmlComponent::statusChanged, this,
        [this](QQmlComponent::Status status)
        {
            switch (status)
            {
                case QQmlComponent::Null:
                    emit statusChanged(QQuickWidget::Null);
                    return;
                case QQmlComponent::Ready:
                {
                    QObject* rootObject = d->qmlComponent->create();
                    d->rootItem.reset(qobject_cast<QQuickItem*>(rootObject));
                    if (!d->rootItem)
                        return;

                    d->rootItem->setParentItem(d->quickWindow->contentItem());

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
    if (d->rootItem)
        d->rootItem.reset();
    d->qmlComponent.reset();
    d->quickWindow.reset();
    d->renderControl.reset();
    if (d->fbo)
        d->fbo.reset();
}

QQuickWindow* GraphicsQmlView::quickWindow() const
{
    return d->quickWindow.data();
}

QQuickItem* GraphicsQmlView::rootObject() const
{
    return d->rootItem.data();
}

QList<QQmlError> GraphicsQmlView::errors() const
{
    return d->qmlComponent->errors();
}

void GraphicsQmlView::Private::scheduleUpdateSizes()
{
    fboTimer.restart();
    updateSizes();
}

void GraphicsQmlView::updateWindowGeometry()
{
    d->scheduleUpdateSizes();
}

QQmlEngine* GraphicsQmlView::engine() const
{
    return d->qmlEngine;
}

void GraphicsQmlView::Private::tryInitializeFbo()
{
    if (!quickWindow)
        return;

    const QSize requiredSize = quickWindow->size() * qApp->devicePixelRatio();

    if (fbo && fbo->size() == requiredSize)
        return;

    if (!fboTimer.isValid())
        fboTimer.start();

    // Avoid frequent re-creation of FrameBuffer objects because it's time consuming.
    if (fbo && !fboTimer.hasExpired(duration_cast<milliseconds>(kFboResizeTimeout).count()))
        return;

    QScopedPointer<QOpenGLFramebufferObject> newFbo(
        new QOpenGLFramebufferObject(
            requiredSize, QOpenGLFramebufferObject::CombinedDepthStencil));

    if (!newFbo->isValid())
        return;

    fbo.swap(newFbo);
    quickWindow->setRenderTarget(fbo.data());

    fboTimer.invalidate();
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
            return QCoreApplication::sendEvent(d->quickWindow.data(), event);
        case QEvent::InputMethod:
            return QCoreApplication::sendEvent(d->quickWindow.data(), event);
        case QEvent::InputMethodQuery:
        {
            bool eventResult = QCoreApplication::sendEvent(d->quickWindow->focusObject(), event);
            // The result in focusObject are based on quickWindow. But
            // the inputMethodTransform won't get updated because the focus
            // is on GraphicsQmlView. We need to remap the value based on the
            // widget.
            remapInputMethodQueryEvent(d->quickWindow->focusObject(), static_cast<QInputMethodQueryEvent*>(event));
            return eventResult;
        }
        case QEvent::GraphicsSceneResize:
        case QEvent::GraphicsSceneMove:
            d->scheduleUpdateSizes();
            event->accept();
            break;
        case QEvent::ShortcutOverride:
            return QCoreApplication::sendEvent(d->quickWindow.data(), event);
        default:
            break;
    }

    return base_type::event(event);
}

void GraphicsQmlView::setSource(const QUrl& url)
{
    d->qmlComponent->loadUrl(url);
}

void GraphicsQmlView::setData(const QByteArray& data, const QUrl& url)
{
    d->qmlComponent->setData(data, url);
}

void GraphicsQmlView::Private::updateSizes()
{
    if (!rootItem || !quickWindow)
        return;

    const auto size = view->size();
    rootItem->setSize(size);

    QPoint offset;
    QWindow* w = renderControl->renderWindow(&offset);
    const QPoint pos = w ? w->mapToGlobal(offset) : offset;

    quickWindow->setGeometry(pos.x(), pos.y(), qCeil(size.width()), qCeil(size.height()));
}

void GraphicsQmlView::Private::ensureOffscreen(const QRectF&, QOpenGLWidget* glWidget)
{
    if (offscreenInitialized)
        return;

    openglContext.reset(new QOpenGLContext());
    openglContext->setShareContext(glWidget->context());
    openglContext->create();
    NX_ASSERT(openglContext->isValid());

    offscreenSurface.reset(new QOffscreenSurface());
    QSurfaceFormat format = openglContext->format();
    // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    offscreenSurface->setFormat(format);
    offscreenSurface->setScreen(openglContext->screen());
    offscreenSurface->create();
    NX_ASSERT(offscreenSurface->isValid());

    openglContext->makeCurrent(offscreenSurface.data());
    renderControl->initialize(openglContext.data());
    openglContext->doneCurrent();

    offscreenInitialized = true;
}

void GraphicsQmlView::Private::ensureVao(QnTextureGLShaderProgram* shader)
{
    if (vaoInitialized)
        return;

    static constexpr GLfloat kTexCoordArray[kQuadArrayLength] = {
        0.0, 0.0,
        1.0, 0.0,
        1.0, 1.0,
        0.0, 1.0
    };

    vertices.create();
    vertices.bind();

    initializeQuadBuffer(shader, "aPosition", &positionBuffer);
    initializeQuadBuffer(shader, "aTexCoord", &texcoordBuffer, kTexCoordArray);

    shader->markInitialized();

    vertices.release();

    vaoInitialized = true;
}

void GraphicsQmlView::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseMove, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseButtonPress, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();
    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseButtonRelease, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseMove, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(d->quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    const QPointF mousePoint = event->pos();
    const QPoint globalPos = event->screenPos();
    QWheelEvent mappedEvent(mousePoint, globalPos, event->delta(), event->buttons(), event->modifiers(), event->orientation());
    QCoreApplication::sendEvent(d->quickWindow.data(), &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::keyPressEvent(QKeyEvent* event)
{
    QCoreApplication::sendEvent(d->quickWindow.data(), event);
}

void GraphicsQmlView::keyReleaseEvent(QKeyEvent* event)
{
    QCoreApplication::sendEvent(d->quickWindow.data(), event);
}

void GraphicsQmlView::focusInEvent(QFocusEvent* event)
{
    QCoreApplication::sendEvent(d->quickWindow.data(), event);
}

void GraphicsQmlView::focusOutEvent(QFocusEvent* event)
{
    // This event may fire during QGraphicsWidget destruction.
    if (!d->quickWindow)
        return;

    QCoreApplication::sendEvent(d->quickWindow.data(), event);
}

bool GraphicsQmlView::focusNextPrevChild(bool next)
{
    QKeyEvent event(QEvent::KeyPress, next ? Qt::Key_Tab : Qt::Key_Backtab, Qt::NoModifier);
    QCoreApplication::sendEvent(d->quickWindow.data(), &event);

    QKeyEvent releaseEvent(QEvent::KeyRelease, next ? Qt::Key_Tab : Qt::Key_Backtab, Qt::NoModifier);
    QCoreApplication::sendEvent(d->quickWindow.data(), &releaseEvent);

    return event.isAccepted();
}

void GraphicsQmlView::Private::paintQml(
    QPainter* painter,
    const QRectF& channelRect,
    QOpenGLWidget* glWidget)
{
    if (!sceneChanged && !renderRequested)
        return;

    QnGlNativePainting::begin(glWidget, painter);

    ensureOffscreen(channelRect, glWidget);

    if (openglContext->makeCurrent(offscreenSurface.data()))
    {
        tryInitializeFbo();

        if (fbo)
        {
            if (sceneChanged)
            {
                renderControl->polishItems();
                renderRequested = renderControl->sync() || renderRequested;
                sceneChanged = false;
            }

            if (renderRequested)
            {
                renderControl->render();
                quickWindow->resetOpenGLState();
                openglContext->functions()->glFlush();
                renderRequested = false;
            }
        }
    }

    QnGlNativePainting::end(painter);
}

void GraphicsQmlView::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (!d->rootItem)
        return;

    const auto glWidget = qobject_cast<QOpenGLWidget*>(scene()->views().first()->viewport());

    auto channelRect = rect();

    d->paintQml(painter, channelRect, glWidget);

    if (!d->fbo)
        return;

    QnGlNativePainting::begin(glWidget, painter);

    auto renderer = QnOpenGLRendererManager::instance(glWidget);
    glWidget->makeCurrent();

    const auto functions = glWidget->context()->functions();
    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    functions->glActiveTexture(GL_TEXTURE0);
    functions->glBindTexture(GL_TEXTURE_2D, d->fbo->texture());

    const auto filter = d->fboTimer.isValid() ? GL_LINEAR : GL_NEAREST;
    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    functions->glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);

    const GLfloat posArray[kQuadArrayLength] = {
        (float)channelRect.left(), (float)channelRect.bottom(),
        (float)channelRect.right(), (float)channelRect.bottom(),
        (float)channelRect.right(), (float)channelRect.top(),
        (float)channelRect.left(), (float)channelRect.top()
    };

    const auto shaderColor = QVector4D(1.0, 1.0, 1.0, painter->opacity());

    auto shader = renderer->getTextureShader();

    d->ensureVao(shader);
    d->positionBuffer.bind();
    d->positionBuffer.write(0, posArray, kQuadArrayLength * sizeof(GLfloat));
    d->positionBuffer.release();

    shader->bind();
    shader->setColor(shaderColor);
    shader->setTexture(0);
    renderer->drawBindedTextureOnQuadVao(&d->vertices, shader);
    shader->release();

    QnGlNativePainting::end(painter);
}

} // namespace nx::vms::client::desktop
