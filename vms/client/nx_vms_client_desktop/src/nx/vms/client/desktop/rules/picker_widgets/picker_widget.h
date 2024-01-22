// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_field.h>
#include <nx/vms/rules/manifest.h>
#include <ui/workbench/workbench_context_aware.h>

#include "../params_widgets/params_widget.h"

namespace nx::vms::client::desktop::rules {

/**
 * Base class for the data pickers. Represents and edit Field's data according to
 * the FieldDescriptor.
 */
class PickerWidget: public QWidget, public SystemContextAware
{
    Q_OBJECT

public:
    PickerWidget(SystemContext* context, ParamsWidget* parent);

    /** Sets field descriptor the picker customization is depends on. */
    void setDescriptor(const vms::rules::FieldDescriptor& descriptor);

    /** Returns field descriptor. Returns nullopt if the descriptor is not set. */
    std::optional<vms::rules::FieldDescriptor> descriptor() const;

    bool hasDescriptor() const;

    virtual void setReadOnly(bool value) = 0;

    virtual void updateUi() = 0;

    ParamsWidget* parentParamsWidget() const;

protected:
    /**
     * Called when descriptor is set. Here derived classes must customize properties
     * according to the fieldDescriptor.
     */
    virtual void onDescriptorSet() = 0;

    std::optional<vms::rules::FieldDescriptor> m_fieldDescriptor;
};

} // namespace nx::vms::client::desktop::rules
