#include "special_layout_panel_widget.h"
#include "ui_special_layout_panel_widget.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>

#include <client/client_globals.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/common/palette.h>
#include <ui/style/custom_style.h>

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

    auto titleFont = ui->captionLabel->font();
    titleFont.setWeight(QFont::Light);
    ui->captionLabel->setFont(titleFont);

    setPaletteColor(this, QPalette::Window, QColor("#1d2327"));
    setAutoFillBackground(true);

    setWidget(body);

    connect(m_layoutResource, &QnLayoutResource::dataChanged,
        this, &SpecialLayoutPanelWidget::handleResourceDataChanged);

    handleResourceDataChanged(Qn::CustomPanelTitleRole);
    handleResourceDataChanged(Qn::CustomPanelDescriptionRole);
    handleResourceDataChanged(Qn::CustomPanelActionsRole);
}

SpecialLayoutPanelWidget::~SpecialLayoutPanelWidget()
{
}

void SpecialLayoutPanelWidget::handleResourceDataChanged(int role)
{
    switch(role)
    {
        case Qn::CustomPanelTitleRole:
        {
            ui->captionLabel->setText(getString(Qn::CustomPanelTitleRole, m_layoutResource));
            break;
        }
        case Qn::CustomPanelDescriptionRole:
        {
            const auto description = getString(Qn::CustomPanelDescriptionRole, m_layoutResource);
            ui->descriptionLabel->setText(description);
            ui->descriptionLabel->setVisible(!description.isEmpty());
            break;
        }
        case Qn::CustomPanelActionsRole:
        {
            updateButtons();
            break;
        }
        default:
            break;
    }
}

void SpecialLayoutPanelWidget::updateButtons()
{
    m_actionButtons.clear();

    const auto actions = m_layoutResource->data(Qn::CustomPanelActionsRole)
        .value<QList<QnActions::IDType>>();

    for (const auto& actionId: actions)
    {
        const auto action = dynamic_cast<QnAction*>(this->action(actionId));
        NX_EXPECT(action);
        if (!action)
            continue;

        const auto button = new QPushButton();
        button->setText(action->text());
        button->setIcon(action->icon());
        switch (action->accent())
        {
            case Qn::ButtonAccent::Standard:
                setAccentStyle(button);
                break;
            case Qn::ButtonAccent::Warning:
                setWarningButtonStyle(button);
                break;
            default:
                break;
        }

        connect(button, &QPushButton::clicked, action, &QAction::trigger);

        m_actionButtons.append(ButtonPtr(button));
        ui->buttonsLayout->addWidget(button, 0, Qt::AlignRight);
    }
}

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
