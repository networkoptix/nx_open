#include "special_layout_panel_widget.h"
#include "ui_special_layout_panel_widget.h"

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

} // namespace

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

    ui(new Ui::SpecialLayoutPanelWidget()),
    m_layoutResource(layoutResource)
{
    setParent(parent);

    const auto body = new QWidget();
    ui->setupUi(body);
    setWidget(body);

    connect(m_layoutResource, &QnLayoutResource::dataChanged,
        this, &SpecialLayoutPanelWidget::handleResourceDataChanged);

    handleResourceDataChanged(Qn::CustomPanelTitleRole);
    handleResourceDataChanged(Qn::CustomPanelDescriptionRole);
    handleResourceDataChanged(Qn::CustomPanelActionsRoleRole);
}

SpecialLayoutPanelWidget::~SpecialLayoutPanelWidget()
{
}

void SpecialLayoutPanelWidget::handleResourceDataChanged(int role)
{
    switch(role)
    {
        case Qn::CustomPanelTitleRole:
            ui->captionLabel->setText(getString(Qn::CustomPanelTitleRole, m_layoutResource));
            break;
        case Qn::CustomPanelDescriptionRole:
            ui->descriptionLabel->setText(getString(Qn::CustomPanelDescriptionRole, m_layoutResource));
            break;
        case Qn::CustomPanelActionsRoleRole:
            updateButtons();
            break;
        default:
            break;
    }
}

void SpecialLayoutPanelWidget::updateButtons()
{
    m_actionButtons.clear();

    const auto actions = m_layoutResource->data(Qn::CustomPanelActionsRoleRole)
        .value<QList<QnActions::IDType>>();

    for (const auto& actionId: actions)
    {
        const auto button = new QToolButton();
        m_actionButtons.append(ButtonPtr(button));
        button->setDefaultAction(action(actionId));
        ui->buttonsLayout->addWidget(button, 0, Qt::AlignRight);
    }
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
