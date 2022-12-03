// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_on_alarm_layout_action_widget.h"
#include "ui_show_on_alarm_layout_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace std::chrono;

QnShowOnAlarmLayoutActionWidget::QnShowOnAlarmLayoutActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::ShowOnAlarmLayoutActionWidget)
{
    ui->setupUi(this);

    connect(ui->forceOpenCheckBox, &QCheckBox::clicked,
        this, &QnShowOnAlarmLayoutActionWidget::paramsChanged);

    connect(ui->rewindForWidget, &RewindForWidget::valueEdited, this,
        [this]()
        {
          if (!model() || m_updating)
              return;

          QScopedValueRollback<bool> guard(m_updating, true);
          auto params = model()->actionParams();
          params.recordBeforeMs = milliseconds(ui->rewindForWidget->value()).count();
          model()->setActionParams(params);
        });

    setSubjectsButton(ui->selectUsersButton);
}

QnShowOnAlarmLayoutActionWidget::~QnShowOnAlarmLayoutActionWidget()
{
}

void QnShowOnAlarmLayoutActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->selectUsersButton);
    setTabOrder(ui->selectUsersButton, ui->forceOpenCheckBox);
    setTabOrder(ui->forceOpenCheckBox, after);
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
        ui->rewindForWidget->setValue(duration_cast<seconds>(
            milliseconds(model()->actionParams().recordBeforeMs)));
    }

    if (fields.testFlag(Field::eventType))
    {
        const bool canUseSource = (model()->eventType() >= vms::api::EventType::userDefinedEvent
            || vms::event::requiresCameraResource(model()->eventType()));
    }
}

void QnShowOnAlarmLayoutActionWidget::paramsChanged()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> updatingRollback(m_updating, true);

    vms::event::ActionParameters params = model()->actionParams();
    params.forced = ui->forceOpenCheckBox->isChecked();
    model()->setActionParams(params);
}
