#include "show_on_alarm_layout_action_widget.h"
#include "ui_show_on_alarm_layout_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <utils/common/scoped_value_rollback.h>

#include <nx/vms/event/action_parameters.h>

using namespace nx;

QnShowOnAlarmLayoutActionWidget::QnShowOnAlarmLayoutActionWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ShowOnAlarmLayoutActionWidget)
{
    ui->setupUi(this);

    connect(ui->forceOpenCheckBox, &QCheckBox::clicked,
        this, &QnShowOnAlarmLayoutActionWidget::paramsChanged);

    connect(ui->useSourceCheckBox, &QCheckBox::clicked,
        this, &QnShowOnAlarmLayoutActionWidget::paramsChanged);

    setSubjectsButton(ui->selectUsersButton);
}

QnShowOnAlarmLayoutActionWidget::~QnShowOnAlarmLayoutActionWidget()
{
}

void QnShowOnAlarmLayoutActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->forceOpenCheckBox);
    setTabOrder(ui->forceOpenCheckBox, ui->useSourceCheckBox);
    setTabOrder(ui->useSourceCheckBox, after);
}

void QnShowOnAlarmLayoutActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    base_type::at_model_dataChanged(fields);

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    if (fields.testFlag(Field::actionParams))
    {
        ui->forceOpenCheckBox->setChecked(model()->actionParams().forced);
        ui->useSourceCheckBox->setChecked(model()->actionParams().useSource);
    }

    if (fields.testFlag(Field::eventType))
    {
        const bool canUseSource = (model()->eventType() >= vms::event::UserDefinedEvent
            || requiresCameraResource(model()->eventType()));
        ui->useSourceCheckBox->setEnabled(canUseSource);
    }
}

void QnShowOnAlarmLayoutActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    vms::event::ActionParameters params = model()->actionParams();
    params.forced = ui->forceOpenCheckBox->isChecked();
    params.useSource = ui->useSourceCheckBox->isChecked();
    model()->setActionParams(params);
}
