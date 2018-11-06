#include "word_wrapped_label.h"

#include <QtGui/QResizeEvent>

namespace {

const int kApproximateRowHeight = 15;

QSize approximateSize(int lines)
{
    if (lines <= 0)
        return QSize();

    return QSize(kApproximateRowHeight, kApproximateRowHeight * lines);
}

} // namespace

QnWordWrappedLabel::QnWordWrappedLabel(QWidget* parent):
    QWidget(parent),
    m_label(new QLabel(this))
{
    m_label->setWordWrap(true);
    connect(m_label, &QLabel::linkActivated, this, &QnWordWrappedLabel::linkActivated);
}

QLabel* QnWordWrappedLabel::label() const
{
    return m_label;
}

void QnWordWrappedLabel::showEvent(QShowEvent* event)
{
    ensureInitialized();
    base_type::showEvent(event);
}

void QnWordWrappedLabel::resizeEvent(QResizeEvent* event)
{
    ensureInitialized();
    m_label->setGeometry(0, 0, event->size().width(), event->size().height());
    updateGeometry();

    base_type::resizeEvent(event);
}

void QnWordWrappedLabel::ensureInitialized()
{
    if (m_initialized)
        return;

    m_initialized = true;
    updateGeometry();
}

QSize QnWordWrappedLabel::sizeHint() const
{
    QSize result(m_label->sizeHint().width(),
        m_label->heightForWidth(this->geometry().width()));

    if (m_initialized && result.isValid())
        return result;

    return approximateSize(m_approximateLines);
}

QSize QnWordWrappedLabel::minimumSizeHint() const
{
    return sizeHint();
}

QString QnWordWrappedLabel::text() const
{
    return m_label->text();
}

void QnWordWrappedLabel::setText(const QString& value)
{
    if (text() == value)
        return;

    m_label->setText(value);
    updateGeometry();
}

Qt::TextFormat QnWordWrappedLabel::textFormat() const
{
    return m_label->textFormat();
}

void QnWordWrappedLabel::setTextFormat(Qt::TextFormat value)
{
    if (textFormat() == value)
        return;

    m_label->setTextFormat(value);
    updateGeometry();
}

int QnWordWrappedLabel::approximateLines() const
{
    return m_approximateLines;
}

void QnWordWrappedLabel::setApproximateLines(int value)
{
    if (m_approximateLines == value)
        return;

    m_approximateLines = value;
    updateGeometry();
}

bool QnWordWrappedLabel::openExternalLinks() const
{
    return m_label->openExternalLinks();
}

void QnWordWrappedLabel::setOpenExternalLinks(bool value)
{
    m_label->setOpenExternalLinks(true);
}
