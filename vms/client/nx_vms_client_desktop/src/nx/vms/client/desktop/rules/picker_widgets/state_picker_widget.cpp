// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_picker_widget.h"

#include <nx/vms/rules/utils/field.h>

namespace nx::vms::client::desktop::rules {

StatePicker::StatePicker(
    vms::rules::StateField* field,
    SystemContext* context,
    ParamsWidget* parent)
    :
    DropdownTextPickerWidgetBase<vms::rules::StateField, StateComboBox>{field, context, parent}
{
    m_label->setText({});

    m_comboBox->setStateStringProvider(
        [](vms::rules::State state)
        {
            switch(state)
            {
                case vms::rules::State::instant:
                    return tr("When event occurs");
                case vms::rules::State::started:
                    return tr("When event starts");
                case vms::rules::State::stopped:
                    return tr("When event stops");
                default:
                    NX_ASSERT(false, "Unexpected state is provided");
                    return QString{};
            }
        });

    m_comboBox->setCurrentIndex(-1);
    m_comboBox->setPlaceholderText(tr("Select state"));
}

void StatePicker::updateUi()
{
    DropdownTextPickerWidgetBase<vms::rules::StateField, StateComboBox>::updateUi();

    const auto eventDurationType = vms::rules::getEventDurationType(
        systemContext()->vmsRulesEngine(), parentParamsWidget()->eventFilter());

    m_comboBox->update(eventDurationType);
    m_comboBox->setCurrentIndex(m_comboBox->findData(QVariant::fromValue(m_field->value())));

    setVisible(m_comboBox->count() > 1);
}

void StatePicker::onActivated()
{
    if (m_comboBox->currentIndex() != -1)
        m_field->setValue(m_comboBox->currentData().value<api::rules::State>());

    DropdownTextPickerWidgetBase<vms::rules::StateField, StateComboBox>::onActivated();
}

} // namespace nx::vms::client::desktop::rules
