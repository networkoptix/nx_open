// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/rules/field.h>
#include <nx/vms/rules/manifest.h>

class QnElidedLabel;

namespace nx::vms::client::desktop::rules {

/**
 * Base class for the data pickers. Represents and edit Field's data according to
 * the FieldDescriptor.
 */
class PickerWidget: public QWidget, public common::SystemContextAware
{
    Q_OBJECT

public:
    PickerWidget(common::SystemContext* context, QWidget* parent = nullptr);

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

    virtual void setReadOnly(bool value);

protected:
    /**
     * Called when descriptor is set. Here derived classes must customize properties
     * according to the fieldDescriptor.
     */
    virtual void onDescriptorSet();

    /**
     * Calls when field and linkedFields are set. Here derived classes must cast the field to
     * the type described in the fieldDescriptor and display field data. Derived classes might
     * be sure field is not nullptr.
     */
    virtual void onFieldsSet();

    QnElidedLabel* m_label{};
    QWidget* m_contentWidget{};
    std::optional<vms::rules::FieldDescriptor> m_fieldDescriptor;
};

template<typename F>
class FieldPickerWidget: public PickerWidget
{
public:
    FieldPickerWidget(common::SystemContext* context, QWidget* parent = nullptr):
        PickerWidget(context, parent)
    {
    }

    virtual void setFields(const QHash<QString, vms::rules::Field*>& fields) override
    {
        if (!NX_ASSERT(m_fieldDescriptor))
            return;

        disconnectFromFields();

        m_field = dynamic_cast<F*>(fields.value(m_fieldDescriptor->fieldName));

        if (m_field)
        {
            for (const auto linkedFieldName: m_fieldDescriptor->linkedFields)
            {
                const auto linkedField = fields.value(linkedFieldName);
                if (NX_ASSERT(linkedField))
                    m_linkedFields[linkedFieldName] = linkedField;
            }

            onFieldsSet();
        }
    }

    template<typename T>
    T* getLinkedField(const char* fieldName) const
    {
        return dynamic_cast<T*>(m_linkedFields.value(fieldName).data());
    }

protected:
    static_assert(std::is_base_of<vms::rules::Field, F>());

    /** The field described in the descriptor, the picker gets value for. */
    QPointer<F> m_field{};

    /** Another fields from the entity(ActionBuilder or EventFilter fields). */
    QHash<QString, QPointer<vms::rules::Field>> m_linkedFields;

private:
    /** Disconnect the picker from the fields. */
    void disconnectFromFields()
    {
        if (m_field)
            m_field->disconnect(this);

        for (const auto& linkedField: m_linkedFields)
        {
            if (linkedField)
                linkedField->disconnect(this);
        }
    }
};

} // namespace nx::vms::client::desktop::rules
