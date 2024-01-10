// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "plain_picker_widget.h"
#include "titled_picker_widget.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename P>
class FieldPickerWidget: public P
{
    static_assert(std::is_base_of<vms::rules::Field, F>());
    static_assert(std::is_base_of<PickerWidget, P>());

public:
    using P::P;

protected:
    template<typename T>
    T* getActionField(const QString& name) const
    {
        return P::parentParamsWidget()->actionBuilder()->template fieldByName<T>(name);
    }

    template<typename T>
    T* getEventField(const QString& name) const
    {
        return P::parentParamsWidget()->eventFilter()->template fieldByName<T>(name);
    }

    std::optional<vms::rules::ItemDescriptor> getEventDescriptor() const
    {
        return P::parentParamsWidget()->eventDescriptor();
    }

    F* theField() const
    {
        auto field = theFieldImpl<F>();
        NX_ASSERT(field);
        return field;
    }

private:
    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::ActionBuilderField, T>::value, void>::type* = nullptr>
    F* theFieldImpl() const
    {
        return getActionField<F>(P::m_fieldDescriptor->fieldName);
    }

    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::EventFilterField, T>::value, void>::type* = nullptr>
    F* theFieldImpl() const
    {
        return getEventField<F>(P::m_fieldDescriptor->fieldName);
    }
};

template<class F>
using PlainFieldPickerWidget = FieldPickerWidget<F, PlainPickerWidget>;

template<class F>
using TitledFieldPickerWidget = FieldPickerWidget<F, TitledPickerWidget>;

} // namespace nx::vms::client::desktop::rules
