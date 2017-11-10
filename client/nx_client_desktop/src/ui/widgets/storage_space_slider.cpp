#include "storage_space_slider.h"

#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

#include <QtWidgets/QStyle>

#include <utils/math/color_transformations.h>
#include <ui/style/helper.h>

#include <nx/client/core/utils/human_readable.h>

QnStorageSpaceSlider::QnStorageSpaceSlider(QWidget *parent):
    base_type(parent),
    m_color(Qt::white)
{
    setOrientation(Qt::Horizontal);
    setMouseTracking(true);
    setProperty(style::Properties::kSliderLength, 0);

    setTextFormat(lit("%1"));

    connect(this, SIGNAL(sliderPressed()), this, SLOT(update()));
    connect(this, SIGNAL(sliderReleased()), this, SLOT(update()));
}

const QColor& QnStorageSpaceSlider::color() const
{
    return m_color;
}

void QnStorageSpaceSlider::setColor(const QColor& color)
{
    m_color = color;
}

QString QnStorageSpaceSlider::text() const
{
    // TODO: #GDM #3.1 move out strings and logic to separate class (string.h:bytesToString)
    const qint64 bytesInMiB = 1024 * 1024;

    if (!m_textFormatHasPlaceholder)
    {
        return m_textFormat;
    }
    else
    {
        if (isSliderDown())
        {
            return formatSize(sliderPosition() * bytesInMiB);
        }
        else
        {
            return lit("%1%").arg(static_cast<int>(relativePosition() * 100));
        }
    }
}

QString QnStorageSpaceSlider::textFormat() const
{
    return m_textFormat;
}

void QnStorageSpaceSlider::setTextFormat(const QString& textFormat)
{
    if (m_textFormat == textFormat)
        return;

    m_textFormat = textFormat;
    m_textFormatHasPlaceholder = textFormat.contains(QLatin1String("%1"));
    update();
}

QString QnStorageSpaceSlider::formatSize(qint64 size)
{
    using HumanReadable = nx::client::core::HumanReadable;

    return HumanReadable::digitalSizePrecise(size,
        HumanReadable::VolumeSize,
        HumanReadable::DigitalSizeMultiplier::Binary,
        HumanReadable::SuffixFormat::Long);
}

void QnStorageSpaceSlider::mouseMoveEvent(QMouseEvent* event)
{
    base_type::mouseMoveEvent(event);

    if (!isEmpty())
    {
        int x = handlePos();
        if (qAbs(x - event->pos().x()) < 6)
        {
            setCursor(Qt::SplitHCursor);
        }
        else
        {
            unsetCursor();
        }
    }
}

void QnStorageSpaceSlider::leaveEvent(QEvent* event)
{
    unsetCursor();

    base_type::leaveEvent(event);
}

void QnStorageSpaceSlider::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    QRect rect = this->rect();

    painter.fillRect(rect, palette().color(backgroundRole()));

    if (!isEmpty())
    {
        int x = handlePos();
        painter.fillRect(QRect(QPoint(0, 0), QPoint(x, rect.bottom())), m_color);

        painter.setPen(withAlpha(m_color.lighter(), 128));
        painter.drawLine(QPoint(x, 0), QPoint(x, rect.bottom()));
    }

    const int textMargin = style()->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, this) + 1;
    QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0);
    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text());
}

qreal QnStorageSpaceSlider::relativePosition() const
{
    return isEmpty() ? 0.0 : static_cast<double>(sliderPosition() - minimum()) / (maximum() - minimum());
}

int QnStorageSpaceSlider::handlePos() const
{
    return rect().width() * relativePosition();
}

bool QnStorageSpaceSlider::isEmpty() const
{
    return maximum() == minimum();
}
