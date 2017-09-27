#include "export_overlay_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QTextDocument>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>

#include <QtWidgets/private/qpixmapfilter_p.h>

#include <utils/common/html.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr qreal kBorderThickness = 2.0;

} // namespace

struct ExportOverlayWidget::Private
{
    Private(ExportOverlayWidget* parent):
        document(new QTextDocument())
    {
        connect(document.data(), &QTextDocument::contentsChanged,
            parent, &ExportOverlayWidget::updateLayout);

        connect(document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            parent, &ExportOverlayWidget::updateLayout);
    }

    QScopedPointer<QTextDocument> document;
    QScopedPointer<QPixmapDropShadowFilter> shadow;
    int overlayWidth = -1; //< -1 means content-defined
    bool borderVisible = true;
    qreal roundingRadius = 0.0;
    qreal opacity = 1.0;
    qreal scale = 1.0;
    QString text;
    QImage image;
    QRectF unscaledRect;

    QPoint initialPos;
    bool dragging = false;

    QPixmap shadowSource;
    QPointF filterOffset;
    int shadowIterations; //< TODO: #vkutin Temporary way to increase shadow density.
};

ExportOverlayWidget::ExportOverlayWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setAttribute(Qt::WA_TranslucentBackground);

    updateLayout();
    updateCursor();

    if (parent)
        parent->installEventFilter(this);
}

ExportOverlayWidget::~ExportOverlayWidget()
{
}

qreal ExportOverlayWidget::scale() const
{
    return d->scale;
}

void ExportOverlayWidget::setScale(qreal value)
{
    if (qFuzzyIsNull(d->scale - value))
        return;

    d->scale = value;
    updateLayout();
}

qreal ExportOverlayWidget::opacity() const
{
    return d->opacity;
}

void ExportOverlayWidget::setOpacity(qreal value)
{
    if (qFuzzyIsNull(d->opacity - value))
        return;

    d->opacity = value;
    update();
}

QString ExportOverlayWidget::text() const
{
    return d->text;
}

void ExportOverlayWidget::setText(const QString& value)
{
    if (d->text == value)
        return;

    d->text = value;

    if (mightBeHtml(value))
        d->document->setHtml(value);
    else
        d->document->setPlainText(value);
}

int ExportOverlayWidget::overlayWidth() const
{
    return d->overlayWidth;
}

void ExportOverlayWidget::setOverlayWidth(int value)
{
    if (d->overlayWidth == value)
        return;

    d->overlayWidth = value;
    d->document->setTextWidth(value);
    updateLayout();
}

int ExportOverlayWidget::textIndent() const
{
    return d->document->documentMargin();
}

void ExportOverlayWidget::setTextIndent(int value)
{
    if (qFuzzyIsNull(d->document->documentMargin() - value))
        return;

    d->document->setDocumentMargin(value);
    updateLayout();
}

QImage ExportOverlayWidget::image() const
{
    return d->image;
}

void ExportOverlayWidget::setImage(const QImage& value)
{
    d->image = value;
    updateLayout();
}

void ExportOverlayWidget::setImage(const QPixmap& value)
{
    setImage(value.toImage());
}

bool ExportOverlayWidget::borderVisible() const
{
    return d->borderVisible;
}

void ExportOverlayWidget::setBorderVisible(bool value)
{
    if (d->borderVisible == value)
        return;

    d->borderVisible = value;
    update();
}

qreal ExportOverlayWidget::roundingRadius() const
{
    return d->roundingRadius;
}

void ExportOverlayWidget::setRoundingRadius(qreal value)
{
    if (qFuzzyIsNull(d->roundingRadius - value))
        return;

    d->roundingRadius = value;
    update();
}

bool ExportOverlayWidget::hasShadow() const
{
    return !d->shadow.isNull();
}

void ExportOverlayWidget::setShadow(const QColor& color, const QPointF& offset, qreal blurRadius,
    int iterations)
{
    if (d->shadow.isNull())
        d->shadow.reset(new QPixmapDropShadowFilter);

    d->shadow->setColor(color);
    d->shadow->setOffset(offset);
    d->shadow->setBlurRadius(blurRadius);
    d->shadowIterations = iterations;

    updateLayout();
}

void ExportOverlayWidget::removeShadow()
{
    if (d->shadow.isNull())
        return;

    d->shadow.reset();
    updateLayout();
}

void ExportOverlayWidget::updateLayout()
{
    QSizeF textSize = (d->text.isEmpty() && !d->image.isNull()) ? QSizeF() : d->document->size();
    QSizeF imageSize = d->image.size();

    if (d->overlayWidth >= 0)
    {
        textSize.setWidth(d->overlayWidth);
        imageSize *= d->overlayWidth / imageSize.width();
    }

    d->unscaledRect.setSize(QSizeF(minimumSize()).expandedTo(textSize).expandedTo(imageSize)
        .expandedTo(QApplication::globalStrut()));

    if (d->shadow)
    {
        const auto effectRect = d->shadow->boundingRectFor(d->unscaledRect);
        d->filterOffset = -effectRect.topLeft();
        d->shadowSource = QPixmap(); //< Invalidate cache.
        d->unscaledRect.setSize(effectRect.size());
    }

    const QRectF geometry(pos(), d->unscaledRect.size() * d->scale);
    setGeometry(geometry.toAlignedRect());
    update();
}

void ExportOverlayWidget::updateCursor()
{
    setCursor(d->dragging ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
}

void ExportOverlayWidget::updatePosition(const QPoint& pos)
{
    int x = pos.x();
    int y = pos.y();

    if (auto parent = parentWidget())
    {
        const auto rect = parent->contentsRect();
        x = qMax(qMin(x, rect.width() - width()), rect.left());
        y = qMax(qMin(y, rect.height() - height()), rect.top());
    }

    move(x, y);
}

void ExportOverlayWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Paint background.
    painter.setBrush(palette().window());
    painter.setPen(Qt::NoPen);
    const auto radius = d->roundingRadius * d->scale;
    painter.drawRoundedRect(rect(), radius, radius);

    // Paint scaled contents.
    painter.scale(d->scale, d->scale);
    painter.setOpacity(d->opacity);

    if (d->shadow)
    {
        if (d->shadowSource.isNull())
        {
            const int devicePixelRatio = qApp->devicePixelRatio();
            const QSize size = d->unscaledRect.toAlignedRect().size();
            d->shadowSource = QPixmap(size * devicePixelRatio);
            d->shadowSource.setDevicePixelRatio(devicePixelRatio);
            d->shadowSource.fill(Qt::transparent);

            QPainter pixmapPainter(&d->shadowSource);
            pixmapPainter.translate(d->filterOffset);
            renderContent(pixmapPainter);
        }

        for (int i = 0; i < d->shadowIterations; ++i)
            d->shadow->draw(&painter, QPointF(), d->shadowSource);
    }
    else
    {
        renderContent(painter);
    }

    // Paint border.
    if (d->borderVisible)
    {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette().link(), kBorderThickness, Qt::DashLine));
        painter.resetTransform();
        painter.setOpacity(1.0);
        painter.drawRect(rect());
    }
}

void ExportOverlayWidget::renderContent(QPainter& painter)
{
    if (!d->image.isNull())
        painter.drawImage(d->unscaledRect, d->image);

    if (!d->text.isEmpty())
    {
        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = palette();
        context.clip = d->unscaledRect;
        d->document->documentLayout()->draw(&painter, context);
    }
}

void ExportOverlayWidget::mousePressEvent(QMouseEvent* event)
{
    d->initialPos = event->pos();
    d->dragging = true;
    updateCursor();
    event->accept();
    emit pressed();
}

void ExportOverlayWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!d->dragging)
        return;

    const auto delta = event->pos() - d->initialPos;
    if (delta.isNull())
        return;

    updatePosition(pos() + delta);
    event->accept();
}

void ExportOverlayWidget::mouseReleaseEvent(QMouseEvent* event)
{
    d->dragging = false;
    updateCursor();
    event->accept();
    emit released();
}

bool ExportOverlayWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parent() && event->type() == QEvent::Resize)
        updatePosition(pos());

    return base_type::eventFilter(watched, event);
}

bool ExportOverlayWidget::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::ParentChange:
        {
            if (auto parent = parentWidget())
                parent->installEventFilter(this);
            break;
        }

        case QEvent::ParentAboutToChange:
        {
            if (auto parent = parentWidget())
                parent->removeEventFilter(this);
            break;
        }

        case QEvent::Resize:
        {
            updatePosition(pos());
            break;
        }

        case QEvent::Show:
        case QEvent::LayoutRequest:
        {
            updateLayout();
            break;
        }

        case QEvent::FontChange:
        {
            d->document->setDefaultFont(font());
            updateLayout();
            break;
        }

        default:
            break;
    }

    return base_type::event(event);
}

} // namespace desktop
} // namespace client
} // namespace nx
