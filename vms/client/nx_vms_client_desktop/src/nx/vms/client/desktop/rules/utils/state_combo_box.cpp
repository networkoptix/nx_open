// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_combo_box.h"

namespace nx::vms::client::desktop::rules {

void StateComboBox::setStateStringProvider(
    std::function<QString(vms::rules::State)> stateStringProvider)
{
    m_stateStringProvider = std::move(stateStringProvider);
}

void StateComboBox::update(vms::rules::EventDurationType eventDurationType)
{
    if (!NX_ASSERT(m_stateStringProvider, "State string provided is not set"))
        return;

    if (!m_eventDurationType || m_eventDurationType != eventDurationType)
    {
        clear();

        if (eventDurationType == vms::rules::EventDurationType::instant
            || eventDurationType == vms::rules::EventDurationType::mixed)
        {
            addItem(
                m_stateStringProvider(api::rules::State::instant),
                QVariant::fromValue(api::rules::State::instant));
        }

        if (eventDurationType == vms::rules::EventDurationType::prolonged
            || eventDurationType == vms::rules::EventDurationType::mixed)
        {
            addItem(
                m_stateStringProvider(api::rules::State::started),
                QVariant::fromValue(api::rules::State::started));
            addItem(
                m_stateStringProvider(api::rules::State::stopped),
                QVariant::fromValue(api::rules::State::stopped));
        }
    }

    m_eventDurationType = eventDurationType;
}

} // namespace nx::vms::client::desktop::rules
