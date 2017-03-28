#include "special_layout_panel_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>

#include <client/client_globals.h>
#include <ui/actions/action_manager.h>

namespace {

QString getString(int role, const QnLayoutResourcePtr& resource)
{
    return resource->data(role).toString();
}

}
namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

SpecialLayoutPanelWidget::SpecialLayoutPanelWidget(
    const QnLayoutResourcePtr& layoutResource,
    QObject* parent)
    :
    base_type(),
    QnWorkbenchContextAware(parent),

    m_caption(new QLabel()),
    m_description(new QLabel()),
    m_layout(new QHBoxLayout()),
    m_layoutResource(layoutResource)
{
    setParent(parent);

    const auto body = new QWidget();
    body->setLayout(m_layout);
    setWidget(body);

    const auto labelsLayout = new QVBoxLayout();
    labelsLayout->addWidget(m_caption);
    labelsLayout->addWidget(m_description);

    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->addLayout(labelsLayout);
    m_layout->addStretch(1000);

    connect(m_layoutResource, &QnLayoutResource::dataChanged,
        this, &SpecialLayoutPanelWidget::handleResourceDataChanged);

    handleResourceDataChanged(Qn::CustomPanelTitleRole);
    handleResourceDataChanged(Qn::CustomPanelDescriptionRole);
    handleResourceDataChanged(Qn::CustomPanelActionsRoleRole);
    handleResourceDataChanged(Qn::LayoutIconRole);
}

void SpecialLayoutPanelWidget::handleResourceDataChanged(int role)
{
    switch(role)
    {
        case Qn::CustomPanelTitleRole:
            m_caption->setText(getString(Qn::CustomPanelTitleRole, m_layoutResource));
            break;
        case Qn::CustomPanelDescriptionRole:
            m_description->setText(getString(Qn::CustomPanelDescriptionRole, m_layoutResource));
            break;
        case Qn::CustomPanelActionsRoleRole:
            updateButtons();
            break;
    }
}

void SpecialLayoutPanelWidget::updateButtons()
{
    static constexpr auto kButtonsStartIndex = 2;
    while(true)
    {
        const auto count = m_layout->count();
        if (count <= kButtonsStartIndex)
            break;

        const auto button = m_layout->itemAt(count - 1)->widget();
        m_layout->removeWidget(button);
    }

    const auto actions = m_layoutResource->data(Qn::CustomPanelActionsRoleRole)
        .value<QList<QnActions::IDType>>();

    for (const auto& actionId: actions)
    {
        const auto button = new QToolButton();
        button->setDefaultAction(action(actionId));
        m_layout->addWidget(button, 0, Qt::AlignRight);
    }
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
