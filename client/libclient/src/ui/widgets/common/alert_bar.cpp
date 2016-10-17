#include "alert_bar.h"

#include <QtWidgets/QLabel>

#include <ui/common/palette.h>
#include <ui/style/helper.h>

QnAlertBar::QnAlertBar(QWidget* parent):
    base_type(parent),
    m_label(new QLabel(this))
{
    setFixedHeight(style::Metrics::kHeaderSize);

    setPaletteColor(this, QPalette::Window, Qt::red); //< sensible default
    m_label->setAutoFillBackground(true);

    m_label->setForegroundRole(QPalette::Text);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_label->setText(QString());
    m_label->setHidden(true);

    m_label->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin, 0,
        style::Metrics::kDefaultTopLevelMargin, 0);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);
}

QString QnAlertBar::text() const
{
    return m_label->text();
}

void QnAlertBar::setText(const QString& text)
{
    if (text == this->text())
        return;

    m_label->setText(text);
    m_label->setHidden(text.isEmpty());
}
