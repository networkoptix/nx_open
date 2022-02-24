// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCheckBox>

namespace nx::vms::client::desktop {

/**
 * An extended check box with additional functionality.
 */
class CheckBox: public QCheckBox
{
    Q_OBJECT
    using base_type = QCheckBox;

public:
    CheckBox(QWidget* parent = nullptr): base_type(parent) {}
    virtual ~CheckBox() override = default;

    // If the check box can be checked by user interaction or only unchecked and indeterminate
    // states are allowed.
    bool canBeChecked() const;
    void setCanBeChecked(bool value);

signals:
    void cannotBeToggled(); //< The check box was clicked but could not be toggled.

protected:
    virtual void nextCheckState() override;

private:
    bool m_canBeChecked = true;
};

} // namespace nx::vms::client::desktop
