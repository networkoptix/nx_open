#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <utils/common/scoped_value_rollback.h>

using namespace nx::client::desktop::ui;

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->settingsButton, &QPushButton::clicked,
        this, &QnPopupBusinessActionWidget::at_settingsButton_clicked);
    connect(ui->forceAcknoledgementCheckBox, &QCheckBox::toggled,
        this, &QnPopupBusinessActionWidget::parametersChanged);

    setSubjectsButton(ui->selectUsersButton);
}

QnPopupBusinessActionWidget::~QnPopupBusinessActionWidget()
{
}

void QnPopupBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    if (fields.testFlag(Field::eventType))
    {
        const auto resourceRequired = nx::vms::event::isResourceRequired(model()->eventType());
        ui->forceAcknoledgementCheckBox->setEnabled(resourceRequired);
    }

    if (fields.testFlag(Field::actionParams))
        ui->forceAcknoledgementCheckBox->setChecked(model()->actionParams().needConfirmation);
}

void QnPopupBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->settingsButton);
    setTabOrder(ui->settingsButton, ui->forceAcknoledgementCheckBox);
    setTabOrder(ui->forceAcknoledgementCheckBox, after);
}

void QnPopupBusinessActionWidget::at_settingsButton_clicked()
{
    menu()->trigger(action::PreferencesNotificationTabAction);
}

void QnPopupBusinessActionWidget::parametersChanged()
{
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    auto params = model()->actionParams();
    params.needConfirmation = ui->forceAcknoledgementCheckBox->isChecked();
    model()->setActionParams(params);
}

