#include "export_overlay_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>

#include <utils/common/html.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr qreal kBorderThickness = 2.0;

QSize textMargins(const QFontMetrics& fontMetrics)
{
    return QSize(fontMetrics.averageCharWidth() / 2, 1);
}

} // namespace

struct ExportOverlayWidget::Private
{
    bool borderVisible = true;
    qreal scale = 1.0;
    QString text;
    QImage image;
    QRectF unscaledRect;

    QPoint initialPos;
    QFont font;
    bool dragging = false;
};

ExportOverlayWidget::ExportOverlayWidget(QWidget* parent):
    base_type(parent),
    d(new Private())
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

QString ExportOverlayWidget::text() const
{
    return d->text;
}

void ExportOverlayWidget::setText(const QString& value)
{
    if (d->text == value)
        return;

    d->text = value;
    update();
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

void ExportOverlayWidget::updateLayout()
{
    QFontMetrics fontMetrics(d->font);
    QSizeF textSize = fontMetrics.size(0, d->text) + textMargins(fontMetrics) * 2;
    QSizeF imageSize = d->image.size();

    d->unscaledRect.setSize(QSizeF(minimumSize()).expandedTo(textSize).expandedTo(imageSize)
        .expandedTo(QApplication::globalStrut()));

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

    // Paint scaled contents.
    painter.scale(d->scale, d->scale);
    renderContent(painter);

    // Paint border.
    if (d->borderVisible)
    {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette().link(), kBorderThickness, Qt::DashLine));
        painter.resetTransform();
        painter.drawRect(rect());
    }
}

void ExportOverlayWidget::renderContent(QPainter& painter)
{
    if (!d->image.isNull())
        painter.drawImage(d->unscaledRect, d->image);

    if (!d->text.isEmpty())
    {
        QPainterPath path;
        QFontMetrics fontMetrics(d->font);
        path.addText(textMargins(fontMetrics).width(), fontMetrics.ascent(), d->font, d->text);
        painter.setRenderHints(QPainter::Antialiasing);
        painter.strokePath(path, QPen(Qt::black, 2.0));
        painter.fillPath(path, Qt::white);
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
            d->font = font();
            d->font.setBold(true);
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
