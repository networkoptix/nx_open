// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "check_box.h"

namespace nx::vms::client::desktop {

bool CheckBox::canBeChecked() const
{
    return m_canBeChecked;
}

void CheckBox::setCanBeChecked(bool value)
{
    if (m_canBeChecked == value)
        return;

    m_canBeChecked = value;

    if (!m_canBeChecked && checkState() == Qt::Checked)
        setCheckState(Qt::Unchecked);
}

void CheckBox::nextCheckState()
{
    if (m_canBeChecked)
    {
        base_type::nextCheckState();
        return;
    }

    if (checkState() == Qt::Unchecked)
    {
        emit cannotBeToggled();
        return;
    }

    setCheckState(Qt::Unchecked);
}

} // namespace nx::vms::client::desktop
