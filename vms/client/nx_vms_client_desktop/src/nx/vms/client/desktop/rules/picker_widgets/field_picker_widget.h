// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_field.h>

#include "plain_picker_widget.h"
#include "titled_picker_widget.h"

namespace nx::vms::client::desktop::rules {

template<typename F, typename P>
class FieldPickerWidget: public P
{
    static_assert(std::is_base_of<vms::rules::Field, F>());
    static_assert(std::is_base_of<PickerWidget, P>());
    static_assert(std::is_base_of<vms::rules::EventFilterField, F>()
        || std::is_base_of<vms::rules::ActionBuilderField, F>());

public:
    using field_type = F;

    template<
        typename BaseWidget = P,
        typename std::enable_if_t<
            std::is_same_v<BaseWidget, PlainPickerWidget>
                || std::is_same_v<BaseWidget, TitledPickerWidget>, bool> = true>
    FieldPickerWidget(F* field, SystemContext* context, ParamsWidget* parent)
        :
        P(field->descriptor()->displayName, context, parent),
        m_field{field}
    {
        NX_ASSERT(m_field);
    }

    template<
        typename BaseWidget = P,
        typename std::enable_if_t<std::is_same_v<BaseWidget, PickerWidget>, bool> = true>
    FieldPickerWidget(F* field, SystemContext* context, ParamsWidget* parent)
        :
        P(context, parent),
        m_field{field}
    {
        NX_ASSERT(m_field);
    }

protected:
    F* m_field{nullptr};

    void updateUi() override
    {
        if (this->isEdited())
            this->setValidity(fieldValidity());
    }

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

    std::optional<vms::rules::ItemDescriptor> getActionDescriptor() const
    {
        return P::parentParamsWidget()->actionDescriptor();
    }

    vms::rules::FieldValidator* fieldValidator() const
    {
        if constexpr (std::is_base_of<nx::vms::rules::ActionBuilderField, field_type>())
        {
            return P::systemContext()->vmsRulesEngine()->actionFieldValidator(m_field->metatype());
        }
        else if constexpr (std::is_base_of<nx::vms::rules::EventFilterField, field_type>())
        {
            return P::systemContext()->vmsRulesEngine()->eventFieldValidator(m_field->metatype());
        }
        else
        {
            static_assert("Unexpected field type");
            return nullptr;
        }
    }

    vms::rules::ValidationResult fieldValidity() const
    {
        if (auto validator = fieldValidator())
            return validator->validity(m_field, P::parentParamsWidget()->rule(), P::systemContext());

        return {};
    }

private:
    void onFieldChanged(const QString& fieldName)
    {
        if (m_field->descriptor()->fieldName == fieldName)
            P::setEdited();
    }
};

template<class F>
using PlainFieldPickerWidget = FieldPickerWidget<F, PlainPickerWidget>;

template<class F>
using TitledFieldPickerWidget = FieldPickerWidget<F, TitledPickerWidget>;

} // namespace nx::vms::client::desktop::rules
