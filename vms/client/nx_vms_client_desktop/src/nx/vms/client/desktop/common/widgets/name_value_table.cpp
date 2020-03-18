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
    virtual QWindow* renderWindow(QPoint* offset) override
    {
        const auto window = m_widget ? m_widget->window()->windowHandle() : nullptr;
        if (offset && window)
            *offset = m_widget->mapTo(m_widget->window(), QPoint(0, 0));

        return window;
    }

    void setWidget(QWidget* value)
    {
        m_widget = value;
    }

private:
    QPointer<QWidget> m_widget;
};

struct SharedRenderer
{
    const QScopedPointer<QOffscreenSurface> surface;
    const QScopedPointer<QOpenGLContext> context;
    const QScopedPointer<RenderControl> renderControl;
    const QScopedPointer<QQuickWindow> quickWindow;
    const QScopedPointer<QQmlComponent> rootComponent;
    const QScopedPointer<QQuickItem> rootItem;

    const auto bindContext()
    {
        const QPointer<QOpenGLContext> previousContext = QOpenGLContext::currentContext();
        const auto previousSurface = previousContext ? previousContext->surface() : nullptr;

        context->makeCurrent(surface.get());
        return nx::utils::makeScopeGuard(
            [this, previousContext, previousSurface]()
            {
                if (previousContext)
                    previousContext->makeCurrent(previousSurface);
                else
                    context->doneCurrent();
            });
    }

    SharedRenderer():
        surface(new QOffscreenSurface()),
        context(new QOpenGLContext()),
        renderControl(new RenderControl()),
        quickWindow(new QQuickWindow(renderControl.get())),
        rootComponent(new QQmlComponent(qnClientCoreModule->mainQmlEngine(),
            QUrl("Internal/NameValueTable.qml"),
            QQmlComponent::CompilationMode::PreferSynchronous)),
        rootItem(qobject_cast<QQuickItem*>(rootComponent->create()))
    {
        QSurfaceFormat format;
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);

        context->setFormat(format);
        context->setShareContext(QOpenGLContext::globalShareContext());
        context->setObjectName("NameValueTableContext");
        NX_ASSERT(context->create());
        surface->setFormat(context->format());
        surface->create();

        NX_CRITICAL(rootItem);
        rootItem->setParentItem(quickWindow->contentItem());
        quickWindow->setColor(Qt::transparent);
        quickWindow->setGeometry({0, 0, 1, 1});

        const auto contextGuard = bindContext();
        renderControl->initialize(QOpenGLContext::currentContext());
    }

    static SharedRenderer& instance()
    {
        static SharedRenderer renderer;
        return renderer;
    }
};

} // namespace

struct NameValueTable::Private
{
    NameValueTable* const q;
    NameValueList content;
    QSize size;

    QScopedPointer<QOpenGLFramebufferObject> fbo;

    QPixmap pixmap;

    Private(NameValueTable* q):
        q(q)
    {
    }

    virtual ~Private()
    {
        const auto contextGuard = SharedRenderer::instance().bindContext();
        fbo.reset();
    }

    void setContent(const NameValueList& value)
    {
        if (content == value)
            return;

        content = value;
        updateImage();
    }

    void setSize(const QSize& value)
    {
        if (size == value)
            return;

        size = value;
        q->updateGeometry();
    }

    void updateImage()
    {
        if (content.isEmpty())
        {
            size = QSize();
            pixmap = QPixmap();
            return;
        }

        auto& r = SharedRenderer::instance();

        QStringList items;
        for (const auto& [name, value]: content)
            items << name << value;

        r.rootItem->setWidth(q->width());
        r.rootItem->setProperty("items", items);
        r.rootItem->setProperty("font", q->font());
        r.rootItem->setProperty("nameColor", q->palette().color(QPalette::WindowText));
        r.rootItem->setProperty("valueColor", q->palette().color(QPalette::Light));

        setSize(QSize(r.rootItem->width(), r.rootItem->height()));
        if (size.isEmpty())
        {
            pixmap = QPixmap();
            return;
        }

        const auto contextGuard = r.bindContext();
        r.quickWindow->setGeometry(QRect({}, size));
        r.renderControl->setWidget(q); //< For screen & devicePixelRatio detection.

        const QSize deviceSize = size * r.quickWindow->effectiveDevicePixelRatio();
        if (!fbo || fbo->size() != deviceSize)
        {
            QOpenGLFramebufferObject::bindDefault();
            fbo.reset(new QOpenGLFramebufferObject(deviceSize,
                QOpenGLFramebufferObject::Attachment::CombinedDepthStencil));
        }

        r.quickWindow->setRenderTarget(fbo.get());
        pixmap = QPixmap::fromImage(r.renderControl->grab());
        r.quickWindow->setRenderTarget(nullptr);
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
    return d->size;
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
