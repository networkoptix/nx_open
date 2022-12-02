// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buzzer_business_action_widget.h"
#include "ui_buzzer_business_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/events/abstract_event.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

namespace nx::vms::client::desktop {

BuzzerBusinessActionWidget::BuzzerBusinessActionWidget(
    SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::BuzzerBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->fixedDurationCheckBox, &QCheckBox::toggled,
        ui->durationWidget, &QWidget::setEnabled);

    connect(ui->fixedDurationCheckBox, &QCheckBox::clicked,
        this, &BuzzerBusinessActionWidget::setParamsToModel);

    connect(ui->durationSpinBox, QnSpinboxIntValueChanged,
        this, &BuzzerBusinessActionWidget::setParamsToModel);

    connect(ui->fixedDurationCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            // In case of instant action event state should be updated.
            if (checked && (model()->eventType() == vms::api::EventType::userDefinedEvent))
                model()->setEventState(vms::api::EventState::undefined);
        });
}

BuzzerBusinessActionWidget::~BuzzerBusinessActionWidget()
{
}

void BuzzerBusinessActionWidget::updateTabOrder(QWidget* before, QWidget* after)
{
    setTabOrder(before, ui->fixedDurationCheckBox);
    setTabOrder(ui->fixedDurationCheckBox, ui->durationSpinBox);
    setTabOrder(ui->durationSpinBox, after);
}

void BuzzerBusinessActionWidget::at_model_dataChanged(Fields fields)
{
    if (!model() || m_updating)
        return;

    base_type::at_model_dataChanged(fields);
    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventType))
    {
        const bool hasToggleState = vms::event::hasToggleState(
            model()->eventType(), model()->eventParams(), systemContext());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);
    }

    if (fields.testFlag(Field::actionParams))
    {
        const auto params = model()->actionParams();

        using namespace std::chrono;
        ui->fixedDurationCheckBox->setChecked(params.durationMs > 0);
        if (ui->fixedDurationCheckBox->isChecked())
        {
            ui->durationSpinBox->setValue(
                duration_cast<seconds>(milliseconds(params.durationMs)).count());
        }
    }
}

void BuzzerBusinessActionWidget::setParamsToModel()
{
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    vms::event::ActionParameters params = model()->actionParams();

    using namespace std::chrono;
    params.durationMs = ui->fixedDurationCheckBox->isChecked()
        ? duration_cast<milliseconds>(seconds(ui->durationSpinBox->value())).count()
        : 0;

    model()->setActionParams(params);
}

} // namespace nx::vms::client::desktop
