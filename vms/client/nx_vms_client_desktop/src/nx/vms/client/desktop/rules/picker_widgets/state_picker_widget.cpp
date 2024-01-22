// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_picker_widget.h"

#include <nx/vms/rules/utils/field.h>

namespace nx::vms::client::desktop::rules {

void StatePicker::onDescriptorSet()
{
    QSignalBlocker blocker{m_comboBox};

    const auto itemFlags = parentParamsWidget()->eventDescriptor()->flags;
    if (itemFlags.testFlag(vms::rules::ItemFlag::instant))
    {
        m_comboBox->addItem(
            tr("When event occurs"),
            QVariant::fromValue(api::rules::State::instant));
    }

    if (itemFlags.testFlag(vms::rules::ItemFlag::prolonged))
    {
        m_comboBox->addItem(
            tr("When event starts"),
            QVariant::fromValue(api::rules::State::started));
        m_comboBox->addItem(
            tr("When event stops"),
            QVariant::fromValue(api::rules::State::stopped));
    }
}

void StatePicker::updateUi()
{
    QSignalBlocker blocker{m_comboBox};
    m_comboBox->setCurrentIndex(m_comboBox->findData(QVariant::fromValue(theField()->value())));
}

void StatePicker::onCurrentIndexChanged()
{
    theField()->setValue(m_comboBox->currentData().value<api::rules::State>());
}

} // namespace nx::vms::client::desktop::rules
