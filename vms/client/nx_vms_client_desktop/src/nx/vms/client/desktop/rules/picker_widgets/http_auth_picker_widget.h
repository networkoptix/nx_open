// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/rules/action_builder_fields/http_auth_field.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class HttpAuthPicker: public FieldPickerWidget<vms::rules::HttpAuthField, PickerWidget>
{
    Q_OBJECT
    using base = FieldPickerWidget<vms::rules::HttpAuthField, PickerWidget>;

public:
    HttpAuthPicker(SystemContext* context, CommonParamsWidget* parent);
    virtual ~HttpAuthPicker() override;

    void setReadOnly(bool value) override;
    void updateUi() override;

protected:
    void onDescriptorSet() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    void onCurrentIndexChanged(int index);
    void onLoginChanged(const QString&  text);
    void onPasswordChanged(const QString&  text);
    void onTokenChanged(const QString&  text);
};

} // namespace nx::vms::client::desktop::rules
