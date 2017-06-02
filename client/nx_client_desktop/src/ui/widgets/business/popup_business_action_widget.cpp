#include "popup_business_action_widget.h"
#include "ui_popup_business_action_widget.h"

#include <nx/vms/event/action_parameters.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

#include <utils/common/scoped_value_rollback.h>

using namespace nx::client::desktop::ui;

QnPopupBusinessActionWidget::QnPopupBusinessActionWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::PopupBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->adminsCheckBox, &QCheckBox::toggled,
        this, &QnPopupBusinessActionWidget::paramsChanged);

    connect(ui->settingsButton, &QPushButton::clicked,
        this, &QnPopupBusinessActionWidget::at_settingsButton_clicked);
}

QnPopupBusinessActionWidget::~QnPopupBusinessActionWidget()
{
}

void QnPopupBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->adminsCheckBox);
    setTabOrder(ui->adminsCheckBox, ui->settingsButton);
    setTabOrder(ui->settingsButton, after);
}

void QnPopupBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model())
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(Field::actionParams))
    {
        ui->adminsCheckBox->setChecked(
            model()->actionParams().userGroup == nx::vms::event::AdminOnly);
    }
}

void QnPopupBusinessActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    nx::vms::event::ActionParameters params;
    params.userGroup = ui->adminsCheckBox->isChecked()
        ? nx::vms::event::AdminOnly
        : nx::vms::event::EveryOne;

    model()->setActionParams(params);
}

void QnPopupBusinessActionWidget::at_settingsButton_clicked()
{
    menu()->trigger(action::PreferencesNotificationTabAction);
}
