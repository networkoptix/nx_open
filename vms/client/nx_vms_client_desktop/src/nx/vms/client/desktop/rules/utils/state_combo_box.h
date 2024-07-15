// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtWidgets/QComboBox>

#include <nx/vms/rules/rules_fwd.h>
#include <nx/vms/rules/utils/event.h>

namespace nx::vms::client::desktop::rules {

class StateComboBox: public QComboBox
{
public:
    using QComboBox::QComboBox;

    void setStateStringProvider(std::function<QString(vms::rules::State)> stateStringProvider);

    void update(vms::rules::EventDurationType eventDurationType);

private:
    std::function<QString(vms::rules::State)> m_stateStringProvider;
    std::optional<vms::rules::EventDurationType> m_eventDurationType;
};

} // namespace nx::vms::client::desktop::rules
