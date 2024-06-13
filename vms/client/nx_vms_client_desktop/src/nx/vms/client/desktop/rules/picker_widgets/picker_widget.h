// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/system_context_aware.h>

#include "../params_widgets/params_widget.h"

namespace nx::vms::client::desktop::rules {

/**
 * Base class for the data pickers. Represents and edit Field's data according to
 * the FieldDescriptor.
 */
class PickerWidget:
    public QWidget,
    public SystemContextAware
{
    Q_OBJECT

public:
    PickerWidget(SystemContext* context, ParamsWidget* parent);

    virtual void setReadOnly(bool value) = 0;
    virtual void updateUi() = 0;

    /** Returns whether the value of the picker was edited by the user. */
    bool isEdited() const;

    /** Mark the widget as edited by a user forcefully. */
    void setEdited();

    ParamsWidget* parentParamsWidget() const;

protected:
    bool m_isEdited{false};

    virtual void setValidity(const vms::rules::ValidationResult& validationResult);
};

} // namespace nx::vms::client::desktop::rules
