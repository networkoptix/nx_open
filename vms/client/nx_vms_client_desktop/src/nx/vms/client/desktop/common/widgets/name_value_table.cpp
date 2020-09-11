#include "name_value_table.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtGui/QGuiApplication>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFramebufferObject>
#include <QtGui/QPainter>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickWindow>

#include <client_core/client_core_module.h>

#include <nx/utils/scope_guard.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/string.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMaxDisplayedGroupValues = 2; //< Before "(+n values)".

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

/**
 * This structure is a singleton. It is created when instance() is called for the first time.
 * It is destroyed when all NameValueTable instances are destroyed or
 * when QApplication::aboutToQuit is sent, whatever happens last.
 */
struct SharedOffscreenRenderer
{
    const QScopedPointer<QOffscreenSurface> surface;
    const QScopedPointer<QOpenGLContext> context;
    QScopedPointer<RenderControl> renderControl;
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

    static std::shared_ptr<SharedOffscreenRenderer> instance()
    {
        return staticInstancePtr();
    }

    ~SharedOffscreenRenderer()
    {
        // Ther's a comment in the Qt sources: "the standard pattern is to destroy
        // the rendercontrol before the QQuickWindow", so lets do it this way.
        renderControl.reset();
    }

private:
    SharedOffscreenRenderer():
        surface(new QOffscreenSurface()),
        context(new QOpenGLContext()),
        renderControl(new RenderControl()),
        quickWindow(new QQuickWindow(renderControl.get())),
        rootComponent(new QQmlComponent(qnClientCoreModule->mainQmlEngine(),
            QUrl("Internal/NameValueTable.qml"),
            QQmlComponent::CompilationMode::PreferSynchronous)),
        rootItem(qobject_cast<QQuickItem*>(rootComponent->create()))
    {
        NX_CRITICAL(qApp && !qApp->closingDown());
        QObject::connect(qApp, &QCoreApplication::aboutToQuit,
            []() { staticInstancePtr().reset(); });

        NX_CRITICAL(rootItem);
        QQmlEngine::setObjectOwnership(rootItem.get(), QQmlEngine::CppOwnership);
        rootItem->setParentItem(quickWindow->contentItem());
        quickWindow->setColor(Qt::transparent);
        quickWindow->setGeometry({0, 0, 1, 1});

        QSurfaceFormat format;
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);

        context->setFormat(format);
        context->setShareContext(QOpenGLContext::globalShareContext());
        context->setObjectName("NameValueTableContext");
        NX_ASSERT(context->create());
        surface->setFormat(context->format());
        surface->create();

        const auto contextGuard = bindContext();
        renderControl->initialize(QOpenGLContext::currentContext());
    }

    static std::shared_ptr<SharedOffscreenRenderer>& staticInstancePtr()
    {
        static std::shared_ptr<SharedOffscreenRenderer> renderer(new SharedOffscreenRenderer());
        return renderer;
    }
};

} // namespace

struct NameValueTable::Private
{
    NameValueTable* const q;
    const std::shared_ptr<SharedOffscreenRenderer> renderer = SharedOffscreenRenderer::instance();

    GroupedValues content;
    QSize size;

    using NameValueList = std::vector<std::pair<QString, QString>>;
    NameValueList nameValueList;

    QScopedPointer<QOpenGLFramebufferObject> fbo;

    QPixmap pixmap;
    nx::utils::PendingOperation updateOp;

    Private(NameValueTable* q):
        q(q)
    {
        updateOp.setIntervalMs(1);
        updateOp.setCallback([this]() { updateImage(); });
        updateOp.setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    }

    virtual ~Private()
    {
        if (!NX_ASSERT(renderer))
            return;

        const auto contextGuard = renderer->bindContext();
        fbo.reset();
    }

    void setContent(const GroupedValues& value)
    {
        if (content == value)
            return;

        content = value;
        updateList();
    }

    QString valuesText(const QStringList& values) const
    {
        const int totalCount = values.size();
        const QString displayedValues = nx::utils::join(values.cbegin(),
            values.cbegin() + std::min(totalCount, kMaxDisplayedGroupValues),
            ", ");

        const int remainder = totalCount - kMaxDisplayedGroupValues;
        if (remainder <= 0)
            return displayedValues;

        return nx::format("%1 <font color=\"%3\">(%2)</font>", displayedValues,
            tr("+%n values", "", remainder),
            q->palette().color(QPalette::WindowText).name());
    }

    void updateList()
    {
        NameValueList newNameValueList;
        newNameValueList.reserve(content.size());

        for (const auto& group: content)
            newNameValueList.push_back({group.name, valuesText(group.values)});

        if (nameValueList == newNameValueList)
            return;

        nameValueList = std::move(newNameValueList);
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
        if (nameValueList.empty() || !NX_ASSERT(renderer))
        {
            size = QSize();
            pixmap = QPixmap();
            return;
        }

        QStringList items;
        for (const auto& [name, value]: nameValueList)
            items << name << value;

        renderer->rootItem->setWidth(q->width());
        renderer->rootItem->setProperty("items", items);
        renderer->rootItem->setProperty("font", q->font());
        renderer->rootItem->setProperty("nameColor", q->palette().color(QPalette::WindowText));
        renderer->rootItem->setProperty("valueColor", q->palette().color(QPalette::Light));

        setSize(QSize(renderer->rootItem->width(), renderer->rootItem->height()));
        if (size.isEmpty())
        {
            pixmap = QPixmap();
            return;
        }

        const auto contextGuard = renderer->bindContext();
        renderer->quickWindow->setGeometry(QRect({}, size));
        renderer->renderControl->setWidget(q); //< For screen & devicePixelRatio detection.

        const QSize deviceSize = size * renderer->quickWindow->effectiveDevicePixelRatio();
        if (!fbo || fbo->size() != deviceSize)
        {
            QOpenGLFramebufferObject::bindDefault();
            fbo.reset(new QOpenGLFramebufferObject(deviceSize,
                QOpenGLFramebufferObject::Attachment::CombinedDepthStencil));
        }

        renderer->quickWindow->setRenderTarget(fbo.get());
        pixmap = QPixmap::fromImage(renderer->renderControl->grab());
        renderer->quickWindow->setRenderTarget(nullptr);
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

NameValueTable::GroupedValues NameValueTable::content() const
{
    return d->content;
}

void NameValueTable::setContent(const GroupedValues& value)
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
            d->updateOp.requestOperation();
            break;

        default:
            break;
    }

    return base_type::event(event);
}

} // namespace nx::vms::client::desktop
