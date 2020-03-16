#include "name_value_table.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtGui/QGuiApplication>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QPainter>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickWindow>

#include <client_core/client_core_module.h>

#include <nx/utils/scope_guard.h>

namespace nx::vms::client::desktop {

namespace {

class RenderControl: public QQuickRenderControl
{
public:
    const auto bindContext() const
    {
        const QPointer<QOpenGLContext> previousContext = QOpenGLContext::currentContext();
        const auto previousSurface = previousContext ? previousContext->surface() : nullptr;

        staticContext().context->makeCurrent(staticContext().surface.get());
        return nx::utils::makeScopeGuard(
            [this, previousContext, previousSurface]()
            {
                if (previousContext)
                    previousContext->makeCurrent(previousSurface);
                else
                    staticContext().context->doneCurrent();
            });
    }

    RenderControl(QWidget* widget):
        QQuickRenderControl(),
        m_widget(widget)
    {
    }

    virtual QWindow* renderWindow(QPoint* offset) override
    {
        const auto window = m_widget ? m_widget->window()->windowHandle() : nullptr;
        if (offset && window)
            *offset = m_widget->mapTo(m_widget->window(), QPoint(0, 0));

        return window;
    }

private:
    struct StaticContext
    {
        const QScopedPointer<QOffscreenSurface> surface;
        const QScopedPointer<QOpenGLContext> context;

        StaticContext():
            surface(new QOffscreenSurface()),
            context(new QOpenGLContext())
        {
            QSurfaceFormat format;
            format.setDepthBufferSize(16);
            format.setStencilBufferSize(8);

            context->setFormat(format);
            context->setShareContext(QOpenGLContext::globalShareContext());
            context->setObjectName("NameValueTableContext");
            NX_ASSERT(context->create());
            surface->setFormat(context->format());
            surface->create();
        }
    };

    static const StaticContext& staticContext()
    {
        static const StaticContext staticContext;
        return staticContext;
    }

private:
    QPointer<QWidget> m_widget;
};

} // namespace

struct NameValueTable::Private
{
    NameValueTable* const q;
    NameValueList content;

    const QScopedPointer<RenderControl> renderControl;

    QScopedPointer<QOpenGLFramebufferObject> fbo;
    const QScopedPointer<QQuickWindow> quickWindow;

    const QScopedPointer<QQmlComponent> rootComponent;
    const QScopedPointer<QQuickItem> rootItem;

    QPixmap pixmap;

    Private(NameValueTable* q):
        q(q),
        renderControl(new RenderControl(q)),
        quickWindow(new QQuickWindow(renderControl.get())),
        rootComponent(new QQmlComponent(qnClientCoreModule->mainQmlEngine(),
            QUrl("Internal/NameValueTable.qml"),
            QQmlComponent::CompilationMode::PreferSynchronous)),
        rootItem(qobject_cast<QQuickItem*>(rootComponent->create()))
    {
        NX_CRITICAL(rootItem);
        rootItem->setParentItem(quickWindow->contentItem());
        quickWindow->setColor(Qt::transparent);
        quickWindow->setGeometry({0, 0, 1, 1});

        connect(quickWindow.get(), &QWindow::widthChanged, q, &QWidget::updateGeometry);
        connect(quickWindow.get(), &QWindow::heightChanged, q, &QWidget::updateGeometry);

        const auto contextGuard = renderControl->bindContext();
        renderControl->initialize(QOpenGLContext::currentContext());
    }

    virtual ~Private()
    {
        const auto contextGuard = renderControl->bindContext();
        fbo.reset();
    }

    void setContent(const NameValueList& value)
    {
        if (content == value)
            return;

        content = value;
        QStringList items;

        for (const auto& [name, value]: content)
            items << name << value;

        rootItem->setProperty("items", items);
        updateImage();
    }

    void updateImage()
    {
        rootItem->setWidth(q->width());
        rootItem->setProperty("font", q->font());
        rootItem->setProperty("nameColor", q->palette().color(QPalette::WindowText));
        rootItem->setProperty("valueColor", q->palette().color(QPalette::Light));

        const QSize size(rootItem->width(), rootItem->height());
        if (size.isEmpty())
        {
            quickWindow->setGeometry({0, 0, 1, 1});
            quickWindow->setRenderTarget(nullptr);
            pixmap = QPixmap();
            return;
        }

        const auto contextGuard = renderControl->bindContext();
        quickWindow->setGeometry(QRect({}, size));

        const QSize deviceSize = size * quickWindow->effectiveDevicePixelRatio();
        if (!fbo || fbo->size() != deviceSize)
        {
            QOpenGLFramebufferObject::bindDefault();
            fbo.reset(new QOpenGLFramebufferObject(deviceSize,
                QOpenGLFramebufferObject::Attachment::CombinedDepthStencil));
        }

        quickWindow->setRenderTarget(fbo.get());
        pixmap = QPixmap::fromImage(renderControl->grab());
        quickWindow->setRenderTarget(nullptr);
        QOpenGLFramebufferObject::bindDefault();
    }
};

NameValueTable::NameValueTable(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
}

NameValueTable::~NameValueTable()
{
    // Required here for forward-declared scoped pointer destruction.
}

NameValueTable::NameValueList NameValueTable::content() const
{
    return d->content;
}

void NameValueTable::setContent(const NameValueList& value)
{
    d->setContent(value);
}

QSize NameValueTable::sizeHint() const
{
    return minimumSizeHint();
}

QSize NameValueTable::minimumSizeHint() const
{
    return d->quickWindow->size();
}

void NameValueTable::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, d->pixmap);
}

bool NameValueTable::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::Resize:
        case QEvent::FontChange:
        case QEvent::PaletteChange:
        case QEvent::ScreenChangeInternal:
            d->updateImage();
            break;

        default:
            break;
    }

    return base_type::event(event);
}

} // namespace nx::vms::client::desktop
