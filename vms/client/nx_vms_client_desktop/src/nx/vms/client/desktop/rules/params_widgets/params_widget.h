// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QVariant>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/rules/field.h>
#include <nx/vms/rules/manifest.h>

namespace nx::vms::client::desktop::rules {

/**
 * Base class for the action and event parameters widgets. Represents a set of data pickers
 * according to the ItemDescriptor.
 */
class ParamsWidget: public QWidget, public SystemContextAware
{
    Q_OBJECT

public:
    ParamsWidget(SystemContext* context, QWidget* parent = nullptr);

    /** Sets descriptor the widget customization is depends on. */
    void setDescriptor(const vms::rules::ItemDescriptor& value);

    const vms::rules::ItemDescriptor& descriptor() const;

    /**
     * Sets fields the widget will interacts with. The fields name and type must correspond
     * to the fields in the descriptor. Must be called after the descriptor is set. Fields that
     * corresponds to the descriptor are passed to picker widgets that display and edit the fields.
     */
    void setFields(const QHash<QString, vms::rules::Field*>& fields);

    /**
     * Set the prolongation of the event or action. Used by some of the widgets to hide some
     * fields.
     */
    virtual void setInstant(bool value);

    virtual void setReadOnly(bool value);

protected:
    /**
     * Calls after the descriptor is set. Here derived classes must customize the widget using
     * the descriptor.
     */
    virtual void onDescriptorSet();

    /**
     * Returns field descriptor by the field name from the item descriptor set. If there is
     * no such a field in the item descriptor returns nullopt.
     */
    std::optional<vms::rules::FieldDescriptor> fieldDescriptor(const QString& fieldName);


private:
    void setupLineEditsPlaceholderColor();

    vms::rules::ItemDescriptor itemDescriptor;
};

} // namespace nx::vms::client::desktop::rules
