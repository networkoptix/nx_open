#include "message_bar.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>

#include <ui/common/palette.h>
#include <ui/style/helper.h>

namespace nx::vms::client::desktop {

MessageBar::MessageBar(QWidget* parent):
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

    m_label->setAttribute(Qt::WA_LayoutOnEntireRect);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    setHidden(!m_reservedSpace);
}

QString MessageBar::text() const
{
    return m_label->text();
}

void MessageBar::setText(const QString& text)
{
    if (text == this->text())
        return;

    m_label->setText(text);
    m_label->setHidden(text.isEmpty());

    updateVisibility();
}

bool MessageBar::reservedSpace() const
{
    return m_reservedSpace;
}

void MessageBar::setReservedSpace(bool reservedSpace)
{
    if (m_reservedSpace == reservedSpace)
        return;

    m_reservedSpace = reservedSpace;
    updateVisibility();
}

QHBoxLayout* MessageBar::getOverlayLayout()
{
    return m_layout;
}

void MessageBar::updateVisibility()
{
    bool hidden = !m_reservedSpace && m_label->isHidden();
    if (hidden == isHidden())
        return;

    setHidden(hidden);

    if (parentWidget() && parentWidget()->layout() && parentWidget()->isVisible())
        parentWidget()->layout()->activate();
}

MultilineAlertBar::MultilineAlertBar(QWidget* parent): base_type(parent)
{
    setPaletteColor(this, QPalette::Window, Qt::red); //< Sensible default.

    label()->setAutoFillBackground(true);
    label()->setForegroundRole(QPalette::Text);

    label()->setContentsMargins(
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding,
        style::Metrics::kDefaultTopLevelMargin, style::Metrics::kStandardPadding);

    setOpenExternalLinks(true);
}

} // namespace nx::vms::client::desktop
