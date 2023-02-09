// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_field.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/client/desktop/common/widgets/widget_with_hint.h>

#include "../params_widgets/common_params_widget.h"

class QnElidedLabel;

namespace nx::vms::client::desktop::rules {

/**
 * Base class for the data pickers. Represents and edit Field's data according to
 * the FieldDescriptor.
 */
class PickerWidget: public QWidget, public SystemContextAware
{
    Q_OBJECT

public:
    PickerWidget(SystemContext* context, CommonParamsWidget* parent);

    /** Sets field descriptor the picker customization is depends on. */
    void setDescriptor(const vms::rules::FieldDescriptor& descriptor);

    /** Returns field descriptor. Returns nullopt if the descriptor is not set. */
    std::optional<vms::rules::FieldDescriptor> descriptor() const;

    bool hasDescriptor() const;

    virtual void setReadOnly(bool value);

protected:
    /**
     * Called when descriptor is set. Here derived classes must customize properties
     * according to the fieldDescriptor.
     */
    virtual void onDescriptorSet();

    virtual void onActionBuilderChanged() = 0;

    virtual void onEventFilterChanged() = 0;

    CommonParamsWidget* parentParamsWidget() const;

    WidgetWithHint<QnElidedLabel>* m_label{nullptr};
    QWidget* m_contentWidget{nullptr};
    std::optional<vms::rules::FieldDescriptor> m_fieldDescriptor;
};

template<typename F>
class FieldPickerWidget: public PickerWidget
{
    static_assert(std::is_base_of<vms::rules::Field, F>());

public:
    using PickerWidget::PickerWidget;

protected:
    template<typename T>
    T* getActionField(const QString& name) const
    {
        return parentParamsWidget()->actionBuilder()->template fieldByName<T>(name);
    }

    template<typename T>
    T* getEventField(const QString& name) const
    {
        return parentParamsWidget()->eventFilter()->template fieldByName<T>(name);
    }

    void onActionBuilderChanged() override
    {
        onActionBuilderChangedImpl<F>();
    }

    void onEventFilterChanged() override
    {
        onEventFilterChangedImpl<F>();
    }

    F* m_field{nullptr};

private:
    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::ActionBuilderField, T>::value, void>::type* = nullptr>
    void onActionBuilderChangedImpl()
    {
        m_field = getActionField<F>(m_fieldDescriptor->fieldName);
    }

    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::EventFilterField, T>::value, void>::type* = nullptr>
    void onActionBuilderChangedImpl()
    {
    }

    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::ActionBuilderField, T>::value, void>::type* = nullptr>
    void onEventFilterChangedImpl()
    {
    }

    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::EventFilterField, T>::value, void>::type* = nullptr>
    void onEventFilterChangedImpl()
    {
        m_field = getEventField<F>(m_fieldDescriptor->fieldName);
    }
};

template <typename F>
class SimpleFieldPickerWidget: public FieldPickerWidget<F>
{
public:
    using FieldPickerWidget<F>::FieldPickerWidget;

protected:
    virtual void updateUi() = 0;

    void onActionBuilderChanged() override
    {
        FieldPickerWidget<F>::onActionBuilderChanged();
        onActionBuilderChangedImpl<F>();
    }

    void onEventFilterChanged() override
    {
        FieldPickerWidget<F>::onEventFilterChanged();
        onEventFilterChangedImpl<F>();
    }

private:
    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::ActionBuilderField, T>::value, void>::type* = nullptr>
    void onActionBuilderChangedImpl()
    {
        updateUi();
    }

    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::EventFilterField, T>::value, void>::type* = nullptr>
    void onActionBuilderChangedImpl()
    {
    }

    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::ActionBuilderField, T>::value, void>::type* = nullptr>
    void onEventFilterChangedImpl()
    {
    }

    template<typename T = F, typename std::enable_if<std::is_base_of<vms::rules::EventFilterField, T>::value, void>::type* = nullptr>
    void onEventFilterChangedImpl()
    {
        updateUi();
    }
};

} // namespace nx::vms::client::desktop::rules
