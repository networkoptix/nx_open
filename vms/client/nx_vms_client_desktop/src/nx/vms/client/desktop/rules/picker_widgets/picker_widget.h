// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>
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
     * Sets fields the picker will use to display and edit. The fields must be consistant with the
     * descriptor, otherwise the field will not be edited.
     */
    virtual void setFields(const QHash<QString, vms::rules::Field*>& fields);

    virtual void setReadOnly(bool value) = 0;

protected:
    /**
     * Called when descriptor is set. Here derived classes must customize properties
     * according to the fieldDescriptor.
     */
    virtual void onDescriptorSet() = 0;

    /**
     * Calls when field and linkedFields are set. Here derived classes must cast the field to
     * the type described in the fieldDescriptor and display field data. Derived classes might
     * be sure field is not nullptr.
     */
    virtual void onFieldsSet();

    std::optional<vms::rules::FieldDescriptor> fieldDescriptor;
};

template<typename F>
class FieldPickerWidget: public PickerWidget
{
public:
    FieldPickerWidget(QWidget* parent = nullptr): PickerWidget(parent){}

    virtual void setFields(const QHash<QString, vms::rules::Field*>& fields) override
    {
        if (!NX_ASSERT(fieldDescriptor))
            return;

        disconnectFromFields();

        field = dynamic_cast<F*>(fields.value(fieldDescriptor->fieldName));

        if (field)
        {
            for (const auto linkedFieldName: fieldDescriptor->linkedFields)
            {
                const auto linkedField = fields.value(linkedFieldName);
                if (NX_ASSERT(linkedField))
                    linkedFields[linkedFieldName] = linkedField;
            }

            onFieldsSet();
        }
    }

protected:
    static_assert(std::is_base_of<vms::rules::Field, F>());

    /** The field described in the descriptor, the picker gets value for. */
    QPointer<F> field{};

    /** Another fields from the entity(ActionBuilder or EventFilter fields). */
    QHash<QString, QPointer<vms::rules::Field>> linkedFields;

private:
    /** Disconnect the picker from the fields. */
    void disconnectFromFields()
    {
        if (field)
            field->disconnect(this);

        for (const auto& linkedField: linkedFields)
        {
            if (linkedField)
                linkedField->disconnect(this);
        }
    }
};

} // namespace nx::vms::client::desktop::rules
