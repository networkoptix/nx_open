#include "special_layout_panel_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QHBoxLayout>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

SpecialLayoutPanelWidget::SpecialLayoutPanelWidget():
    base_type(),
    m_captionLabel(new QLabel()),
    m_layout(new QHBoxLayout())
{
    const auto body = new QWidget();
    body->setLayout(m_layout);
    setWidget(body);

    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->addWidget(m_captionLabel);
    m_layout->addStretch(1);

    setOpacity(0.5);
}

void SpecialLayoutPanelWidget::setCaption(const QString& caption)
{
    m_captionLabel->setText(caption);
}

void SpecialLayoutPanelWidget::addButton(QAbstractButton* button)
{
    m_layout->addWidget(button, 0, Qt::AlignRight);
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
