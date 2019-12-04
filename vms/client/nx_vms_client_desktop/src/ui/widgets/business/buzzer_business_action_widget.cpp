#include "buzzer_business_action_widget.h"

#include "ui_buzzer_business_action_widget.h"

#include <nx/vms/event/action_parameters.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <QtCore/QScopedValueRollback>

namespace nx::vms::client::desktop {

BuzzerBusinessActionWidget::BuzzerBusinessActionWidget(QWidget* parent):
    base_type(parent),
    m_ui(new Ui::BuzzerBusinessActionWidget)
{
    m_ui->setupUi(this);

    connect(m_ui->fixedDurationCheckBox, &QCheckBox::toggled,
        m_ui->durationWidget, &QWidget::setEnabled);

    connect(m_ui->fixedDurationCheckBox, &QCheckBox::clicked,
        this, &BuzzerBusinessActionWidget::setParamsToModel);

    connect(m_ui->durationSpinBox, QnSpinboxIntValueChanged,
        this, &BuzzerBusinessActionWidget::setParamsToModel);

    connect(m_ui->fixedDurationCheckBox, &QCheckBox::toggled, this,
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
    setTabOrder(before, m_ui->fixedDurationCheckBox);
    setTabOrder(m_ui->fixedDurationCheckBox, m_ui->durationSpinBox);
    setTabOrder(m_ui->durationSpinBox, after);
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
            model()->eventType(), model()->eventParams(), commonModule());
        if (!hasToggleState)
            m_ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(m_ui->fixedDurationCheckBox, !hasToggleState);
    }

    if (fields.testFlag(Field::actionParams))
    {
        const auto params = model()->actionParams();

        using namespace std::chrono;
        m_ui->fixedDurationCheckBox->setChecked(params.durationMs > 0);
        if (m_ui->fixedDurationCheckBox->isChecked())
        {
            m_ui->durationSpinBox->setValue(
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
    params.durationMs = m_ui->fixedDurationCheckBox->isChecked()
        ? duration_cast<milliseconds>(seconds(m_ui->durationSpinBox->value())).count()
        : 0;

    model()->setActionParams(params);
}

} // namespace nx::vms::client::desktop
