// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <nx/utils/impl_ptr.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>

#include "multiline_text_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class TextWithFieldsPicker: public MultilineTextPickerWidget<vms::rules::TextWithFields>
{
    using base = MultilineTextPickerWidget<vms::rules::TextWithFields>;
public:
    TextWithFieldsPicker(
        vms::rules::TextWithFields* field, SystemContext* context, ParamsWidget* parent);
    virtual ~TextWithFieldsPicker() override;

    void updateUi() override;

protected:
    void setValidity(const vms::rules::ValidationResult& validationResult) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::rules
