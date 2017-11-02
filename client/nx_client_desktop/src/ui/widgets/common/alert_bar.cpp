#include "alert_bar.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>

#include <ui/common/palette.h>
#include <ui/style/helper.h>

QnAlertBar::QnAlertBar(QWidget* parent):
    base_type(parent),
    m_label(new QLabel(this)),
    m_layout(new QHBoxLayout(this))
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    setPaletteColor(this, QPalette::Window, Qt::red); //< Sensible default.
    m_label->setLayout(m_layout);
    m_label->setAutoFillBackground(true);

    m_label->setForegroundRole(QPalette::Text);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_label->setText(QString());
    m_label->setHidden(true);
    m_label->setOpenExternalLinks(true);

    m_label->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding,
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    setHidden(!m_reservedSpace);
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

    updateVisibility();
}

bool QnAlertBar::reservedSpace() const
{
    return m_reservedSpace;
}

void QnAlertBar::setReservedSpace(bool reservedSpace)
{
    if (m_reservedSpace == reservedSpace)
        return;

    m_reservedSpace = reservedSpace;
    updateVisibility();
}

QHBoxLayout* QnAlertBar::getOverlayLayout()
{
    return m_layout;
}

void QnAlertBar::updateVisibility()
{
    bool hidden = !m_reservedSpace && m_label->isHidden();
    if (hidden == isHidden())
        return;

    setHidden(hidden);

    if (parentWidget() && parentWidget()->layout() && parentWidget()->isVisible())
        parentWidget()->layout()->activate();
}
