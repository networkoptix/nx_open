// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::desktop::rules {

#define PICKER_WIDGET_COMMON_USINGS using FieldPickerWidget<F>::m_label;\
    using FieldPickerWidget<F>::m_contentWidget;\
    using FieldPickerWidget<F>::m_fieldDescriptor;\
    using FieldPickerWidget<F>::m_field;\
    using FieldPickerWidget<F>::connect;\
    using FieldPickerWidget<F>::resourcePool;

} // namespace nx::vms::client::desktop::rules
