#include "word_wrapped_label.h"

QnWordWrappedLabel::QnWordWrappedLabel(QWidget *parent /*= 0*/):
    QWidget(parent),
    m_label(new QLabel(this))
{
    m_label->setWordWrap(true);
}

QLabel* QnWordWrappedLabel::label() const {
    return m_label;
}

void QnWordWrappedLabel::resizeEvent(QResizeEvent * event) {
    m_label->setGeometry(0, 0, event->size().width(), event->size().height());
    updateGeometry();
}

QSize QnWordWrappedLabel::sizeHint() const {
    return QSize(
        m_label->sizeHint().width(),
        m_label->heightForWidth(this->geometry().width()));
}

QSize QnWordWrappedLabel::minimumSizeHint() const {
    return sizeHint();
}

QString QnWordWrappedLabel::text() const {
    return m_label->text();
}

void QnWordWrappedLabel::setText(const QString &value) {
    m_label->setText(value);
    updateGeometry();
}
