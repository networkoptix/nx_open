#include "optional_label.h"

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>

namespace nx::vms::client::desktop {

OptionalLabel::OptionalLabel(QWidget* parent):
    base_type(parent),
    m_placeholderLabel(new QLabel(this))
{
    anchorWidgetToParent(m_placeholderLabel);
    m_placeholderLabel->setText(QString::fromWCharArray(L"\x2013\x2013\x2013\x2013"));
    m_placeholderLabel->setForegroundRole(QPalette::WindowText);
}

OptionalLabel::~OptionalLabel()
{
}

QString OptionalLabel::placeholderText() const
{
    return m_placeholderLabel->text();
}

void OptionalLabel::setPlaceholderText(const QString& value)
{
    m_placeholderLabel->setText(value);
}

QPalette::ColorRole OptionalLabel::placeholderRole() const
{
    return m_placeholderLabel->foregroundRole();
}

void OptionalLabel::setPlaceholderRole(QPalette::ColorRole value)
{
    m_placeholderLabel->setForegroundRole(value);
}

void OptionalLabel::paintEvent(QPaintEvent* event)
{
    m_placeholderLabel->setVisible(text().isEmpty());
    base_type::paintEvent(event);
}

QSize OptionalLabel::sizeHint() const
{
    const auto result = base_type::sizeHint();
    return text().isEmpty()
        ? result.expandedTo(m_placeholderLabel->sizeHint())
        : result;
}

QSize OptionalLabel::minimumSizeHint() const
{
    const auto result = base_type::minimumSizeHint();
    return text().isEmpty()
        ? result.expandedTo(m_placeholderLabel->minimumSizeHint())
        : result;
}

} // namespace nx::vms::client::desktop
