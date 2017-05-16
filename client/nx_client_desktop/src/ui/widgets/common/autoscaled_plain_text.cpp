#include "autoscaled_plain_text.h"

#include <QtGui/QPainter>

#include <ui/common/geometry.h>
#include <utils/common/event_processors.h>


class QnAutoscaledPlainTextPrivate: public QObject
{
    Q_DECLARE_PUBLIC(QnAutoscaledPlainText);
    QnAutoscaledPlainText* q_ptr;

public:
    QnAutoscaledPlainTextPrivate(QnAutoscaledPlainText* q):
        QObject(),
        q_ptr(q),
        alignment(Qt::AlignCenter),
        weightScaled(true),
        m_effectiveFontDirty(true)
    {
        installEventHandler(q, { QEvent::Resize, QEvent::ContentsRectChange },
            this, &QnAutoscaledPlainTextPrivate::invalidateEffectiveFont);
        installEventHandler(q, QEvent::FontChange,
            this, &QnAutoscaledPlainTextPrivate::invalidate);
    }

    void invalidateEffectiveFont()
    {
        m_effectiveFontDirty = true;
    }

    void invalidate()
    {
        Q_Q(QnAutoscaledPlainText);
        auto newSizeHint = q->fontMetrics().size(0, text);
        if (newSizeHint == sizeHint)
            return;

        sizeHint = newSizeHint;
        invalidateEffectiveFont();
        q->updateGeometry();
        q->update();
    }

    const QFont& effectiveFont() const
    {
        if (!m_effectiveFontDirty)
            return m_effectiveFont;

        Q_Q(const QnAutoscaledPlainText);
        auto sizeToFit = q->contentsRect().size();

        qreal factor = 1.0 / QnGeometry::scaleFactor(sizeHint, sizeToFit, Qt::KeepAspectRatio);

        m_effectiveFont = q->font();

        if (factor > 1.0)
        {
            /* Scale font: */
            int pixelSize = m_effectiveFont.pixelSize();
            if (pixelSize != -1)
                m_effectiveFont.setPixelSize(static_cast<int>(pixelSize / factor));
            else
                m_effectiveFont.setPointSizeF(m_effectiveFont.pointSizeF() / factor);

            if (weightScaled)
            {
                /* Do some sensible addition to font weight: */
                int weight = m_effectiveFont.weight() + static_cast<int>(
                    (QFont::Bold - QFont::Normal) * (factor - 1.0));
                // Qt allows to set weight only in range [0..99]
                weight = qBound(0, weight, 99);
                m_effectiveFont.setWeight(weight);
            }
        }

        m_effectiveFontDirty = false;
        return m_effectiveFont;
    }

public:
    QString text;
    Qt::Alignment alignment;
    QSize sizeHint;
    bool weightScaled;

private:
    mutable QFont m_effectiveFont;
    mutable bool m_effectiveFontDirty;
};

QnAutoscaledPlainText::QnAutoscaledPlainText(QWidget* parent) :
    base_type(parent),
    d_ptr(new QnAutoscaledPlainTextPrivate(this))
{
    setAutoFillBackground(false);
    setForegroundRole(QPalette::WindowText);
}

QnAutoscaledPlainText::~QnAutoscaledPlainText()
{
}

QString QnAutoscaledPlainText::text() const
{
    Q_D(const QnAutoscaledPlainText);
    return d->text;
}

void QnAutoscaledPlainText::setText(const QString& text)
{
    Q_D(QnAutoscaledPlainText);
    if (d->text == text)
        return;

    d->text = text;
    d->invalidate();
}

Qt::Alignment QnAutoscaledPlainText::alignment() const
{
    Q_D(const QnAutoscaledPlainText);
    return d->alignment;
}

void QnAutoscaledPlainText::setAlignment(Qt::Alignment alignment)
{
    Q_D(QnAutoscaledPlainText);
    if (d->alignment == alignment)
        return;

    d->alignment = alignment;
    update();
}

bool QnAutoscaledPlainText::weightScaled() const
{
    Q_D(const QnAutoscaledPlainText);
    return d->weightScaled;
}

void QnAutoscaledPlainText::setWeightScaled(bool value)
{
    Q_D(QnAutoscaledPlainText);
    if (d->weightScaled == value)
        return;

    d->weightScaled = value;
    d->invalidate();
}

QFont QnAutoscaledPlainText::effectiveFont() const
{
    Q_D(const QnAutoscaledPlainText);
    return d->effectiveFont();
}

QSize QnAutoscaledPlainText::sizeHint() const
{
    Q_D(const QnAutoscaledPlainText);
    auto m = contentsMargins();
    return d->sizeHint + QSize(
        m.left() + m.right(),
        m.top() + m.bottom());
}

void QnAutoscaledPlainText::paintEvent(QPaintEvent* event)
{
    Q_D(QnAutoscaledPlainText);
    base_type::paintEvent(event);

    if (d->text.isEmpty())
        return;

    QPainter painter(this);
    painter.setFont(effectiveFont());
    painter.setPen(palette().color(foregroundRole()));
    painter.drawText(contentsRect(), d->alignment, d->text);
}
