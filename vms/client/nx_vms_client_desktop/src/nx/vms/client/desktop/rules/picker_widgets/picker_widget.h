// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/rules/field.h>
#include <nx/vms/rules/manifest.h>

namespace nx::vms::client::desktop::rules {

/**
 * Base class for the data pickers. Represents and edit Field's data according to
 * the FieldDescriptor.
 */
class PickerWidget: public QWidget
{
    Q_OBJECT

public:
    explicit PickerWidget(QWidget* parent = nullptr);

    /** Sets field descriptor the picker customization is depends on. */
    void setDescriptor(const vms::rules::FieldDescriptor& descriptor);

    /** Returns field descriptor. Returns nullopt if the descriptor is not set. */
    std::optional<vms::rules::FieldDescriptor> descriptor() const;

    bool hasDescriptor() const;

    /**
     * Sets field the picker will use to display and edit. The field must be consistant with the
     * descriptor, otherwise the field will not be edited.
     */
    virtual void setField(vms::rules::Field* value);

    virtual void setReadOnly(bool value) = 0;

signals:
    /** Emits whenever the field is edited. */
    void edited(); //< TODO: Probably Field class must notify about it changes.

protected:
    /**
     * Called when descriptor is set. Here derived classes must customise properties
     * according to the fieldDescriptor.
     */
    virtual void onDescriptorSet() = 0;

    /**
     * Calls when field is set. Here derived classes must cast the field to the type described
     * in the fieldDescriptor and display field data. Derived classes might be sure field is
     * not nullptr.
     */
    virtual void onFieldSet();

    std::optional<vms::rules::FieldDescriptor> fieldDescriptor;
};

template<typename F>
class FieldPickerWidget: public PickerWidget
{
public:
    FieldPickerWidget(QWidget* parent = nullptr): PickerWidget(parent){}

    void setField(vms::rules::Field* value) override
    {
        field = dynamic_cast<F*>(value);

        if (field)
            onFieldSet();
    }

protected:
    static_assert(std::is_base_of<vms::rules::Field, F>());
    F* field{};
};

} // namespace nx::vms::client::desktop::rules
