#include "graphics_qml_view.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QQuickRenderControl>
#include <QQuickItem>
#include <QOpenGLContext>
#include <QtGui/QOpenGLFunctions_1_5>
#include <QOpenGLFunctions>

#include <QGraphicsSceneHoverEvent>
#include <QApplication>

#include <ui/workaround/gl_native_painting.h>

namespace {

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
    // No more than one of these 3 can be set
    if (states & Qt::WindowMinimized)
        return Qt::WindowMinimized;
    if (states & Qt::WindowMaximized)
        return Qt::WindowMaximized;
    if (states & Qt::WindowFullScreen)
        return Qt::WindowFullScreen;

    // No state means "windowed" - we ignore Qt::WindowActive
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

} // anonymous namespace

namespace nx::vms::client::desktop {

class RenderControl : public QQuickRenderControl
{
    using base_type = QQuickRenderControl;

public:
    RenderControl(GraphicsQmlView* parent)
        : QQuickRenderControl(parent), m_graphicsQmlView(parent), m_monitoredWidget(nullptr) {}

    ~RenderControl();

    virtual QWindow* renderWindow(QPoint* offset) override;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        switch(event->type())
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
    QWidget* m_monitoredWidget;
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

GraphicsQmlView::GraphicsQmlView(QGraphicsItem* parent, Qt::WindowFlags wFlags)
    : QGraphicsWidget(parent, wFlags)
{
    m_initialized = false;
    m_fbo = nullptr;
    m_rootItem = nullptr;

    m_renderControl = new RenderControl(this);
    connect( m_renderControl, &QQuickRenderControl::renderRequested, this,
        [this]()
        {
            this->update();
        }
    );
    connect( m_renderControl, &QQuickRenderControl::sceneChanged, this,
        [this]()
        {
            this->update();
        }
    );

    m_quickWindow = new QQuickWindow(m_renderControl);
    m_quickWindow->setTitle(QString::fromLatin1("Offscreen"));

    m_quickWindow->setGeometry(0, 0, 1920, 1080);

    connect(m_quickWindow, &QQuickWindow::sceneGraphInitialized, this,
        [this]()
        {
            ensureFbo();
        }
    );

    m_qmlEngine = new QQmlEngine();
    if (!m_qmlEngine->incubationController())
        m_qmlEngine->setIncubationController(m_quickWindow->incubationController());

    m_qmlComponent = new QQmlComponent(m_qmlEngine);
    connect(m_qmlComponent, &QQmlComponent::statusChanged, this, &GraphicsQmlView::componentStatusChanged);
    connect(m_qmlComponent, &QQmlComponent::statusChanged, this, &GraphicsQmlView::statusChanged);

    setAcceptHoverEvents(true);
    setFocusPolicy(Qt::StrongFocus);
}

GraphicsQmlView::~GraphicsQmlView()
{
    if (m_rootItem)
        delete m_rootItem;
    delete m_qmlComponent;
    delete m_qmlEngine;
    delete m_quickWindow;
    delete m_renderControl;
    if (m_fbo)
        delete m_fbo;
}

QQmlEngine* GraphicsQmlView::engine() const
{
    return m_qmlEngine;
}

void GraphicsQmlView::ensureFbo()
{
    if (!m_quickWindow)
        return;

    QSize fboSize = m_quickWindow->size() * qApp->devicePixelRatio();

    if (m_fbo && m_fbo->size() != fboSize)
    {
        delete m_fbo;
        m_fbo = nullptr;
    }

    if (!m_fbo)
    {
        m_fbo = new QOpenGLFramebufferObject(fboSize, QOpenGLFramebufferObject::CombinedDepthStencil);
        m_quickWindow->setRenderTarget(m_fbo);
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
            return QCoreApplication::sendEvent(m_quickWindow, event);
        case QEvent::InputMethod:
            return QCoreApplication::sendEvent(m_quickWindow, event);
        case QEvent::InputMethodQuery:
        {
            bool eventResult = QCoreApplication::sendEvent(m_quickWindow->focusObject(), event);
            // The result in focusObject are based on m_quickWindow. But
            // the inputMethodTransform won't get updated because the focus
            // is on GraphicsQmlView. We need to remap the value based on the
            // widget.
            remapInputMethodQueryEvent(m_quickWindow->focusObject(), static_cast<QInputMethodQueryEvent*>(event));
            return eventResult;
        }
        case QEvent::GraphicsSceneResize:
            updateSizes();
            event->accept();
            break;
        case QEvent::ShortcutOverride:
            return QCoreApplication::sendEvent(m_quickWindow, event);
        default:
            break;
    }

    return base_type::event(event);
}

void GraphicsQmlView::setSource(const QUrl& url)
{
    m_qmlComponent->loadUrl(url);
}

void GraphicsQmlView::setData(const QByteArray& data, const QUrl& url)
{
    m_qmlComponent->setData(data, url);
}

void GraphicsQmlView::componentStatusChanged(QQmlComponent::Status status)
{
    if (m_qmlComponent->isError())
    {
        const QList<QQmlError> errorList = m_qmlComponent->errors();
        for (const QQmlError &error : errorList)
            qDebug() << error.url() << error.line() << error;
        return;
    }

    if( QQmlComponent::Ready != status)
        return;

    QObject* rootObject = m_qmlComponent->create();
    if (m_qmlComponent->isError())
    {
        const QList<QQmlError> errorList = m_qmlComponent->errors();
        for (const QQmlError &error : errorList)
            qDebug() << error.url() << error.line() << error;
        return;
    }
    m_rootItem = qobject_cast<QQuickItem*>(rootObject);
    if (!m_rootItem)
        return;

    m_rootItem->setParentItem(m_quickWindow->contentItem());

    updateSizes();
}

void GraphicsQmlView::updateSizes()
{
    if (!m_rootItem || !m_quickWindow)
        return;

    m_rootItem->setSize(size());

    QPoint offset;
    QWindow* w = m_renderControl->renderWindow(&offset);
    const QPoint pos = w ? w->mapToGlobal(offset) : offset;

    m_quickWindow->setGeometry(pos.x(), pos.y(), size().width(), size().height());
}

void GraphicsQmlView::initialize(const QRectF& rect, QOpenGLWidget* glWidget)
{
    auto context = glWidget->context();

    m_offscreenSurface = new QOffscreenSurface();
    QSurfaceFormat format = context->format();
    // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(8);

    m_offscreenSurface->setFormat(format);
    m_offscreenSurface->setScreen(context->screen());
    m_offscreenSurface->create();

    context->makeCurrent(m_offscreenSurface);
    m_renderControl->initialize(context);
    context->doneCurrent();

    m_initialized = true;
}

void GraphicsQmlView::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = Qt::NoButton;
    Qt::MouseButtons buttons = Qt::NoButton;
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseMove, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(m_quickWindow, &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseButtonPress, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(m_quickWindow, &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();
    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseButtonRelease, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(m_quickWindow, &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF mousePoint = event->pos();

    Qt::MouseButton button = event->button();
    Qt::MouseButtons buttons = event->buttons();
    Qt::KeyboardModifiers modifiers = event->modifiers();

    QMouseEvent mappedEvent(QEvent::MouseMove, mousePoint, mousePoint, event->screenPos(), button, buttons, modifiers);
    QCoreApplication::sendEvent(m_quickWindow, &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    const QPointF mousePoint = event->pos();
    const QPoint globalPos = event->screenPos();
    QWheelEvent mappedEvent(mousePoint, globalPos, event->delta(), event->buttons(), event->modifiers(), event->orientation());
    QCoreApplication::sendEvent(m_quickWindow, &mappedEvent);
    event->setAccepted(mappedEvent.isAccepted());
}

void GraphicsQmlView::keyPressEvent(QKeyEvent* event)
{
    QCoreApplication::sendEvent(m_quickWindow, event);
}

void GraphicsQmlView::keyReleaseEvent(QKeyEvent* event)
{
    QCoreApplication::sendEvent(m_quickWindow, event);
}

void GraphicsQmlView::focusInEvent(QFocusEvent* event)
{
    QCoreApplication::sendEvent(m_quickWindow, event);
}

void GraphicsQmlView::focusOutEvent(QFocusEvent* event)
{
    QCoreApplication::sendEvent(m_quickWindow, event);
}

void GraphicsQmlView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    if (!m_rootItem)
        return;

    const auto glWidget = qobject_cast<QOpenGLWidget*>(scene()->views().first()->viewport());

    auto channelRect = rect();

    QnGlNativePainting::begin(glWidget, painter);

    if (!m_initialized)
        initialize(channelRect, glWidget);

    ensureFbo();

    if(glWidget->context()->makeCurrent(m_offscreenSurface))
    {
        m_renderControl->polishItems();
        m_renderControl->sync();

        m_renderControl->render();
        m_quickWindow->resetOpenGLState();
        glWidget->context()->functions()->glFlush();
        glWidget->context()->doneCurrent();
    }

    QnGlNativePainting::end(painter);

    if (!m_fbo)
        return;

    QnGlNativePainting::begin(glWidget, painter);

    const auto functions = QOpenGLContext::currentContext()->versionFunctions<QOpenGLFunctions_1_5>();

    // const float pixels[] = {0.0, 1.0, 1.0, 0.0};
    // functions->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RED, GL_FLOAT, pixels);

    functions->glBindTexture(GL_TEXTURE_2D, m_fbo->texture());

    functions->glEnable(GL_TEXTURE_2D);
    functions->glBegin(GL_QUADS);
    functions->glColor3f(1, 1, 1);
    functions->glTexCoord2d(0, 0);
    functions->glVertex2d(channelRect.left(), channelRect.bottom());
    functions->glTexCoord2d(0, 1);
    functions->glVertex2d(channelRect.left(), channelRect.top());
    functions->glTexCoord2d(1, 1);
    functions->glVertex2d(channelRect.right(), channelRect.top());
    functions->glTexCoord2d(1, 0);
    functions->glVertex2d(channelRect.right(), channelRect.bottom());
    functions->glEnd();

    QnGlNativePainting::end(painter);
}

} // namespace nx::vms::client::desktop
