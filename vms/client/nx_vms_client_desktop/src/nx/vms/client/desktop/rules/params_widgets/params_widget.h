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
    explicit ParamsWidget(SystemContext* context, QWidget* parent = nullptr);

    /** Sets descriptor the widget customization is depends on. */
    void setDescriptor(const vms::rules::ItemDescriptor& value);

    const vms::rules::ItemDescriptor& descriptor() const;

    void setActionBuilder(vms::rules::ActionBuilder* actionBuilder);
    vms::rules::ActionBuilder* actionBuilder() const;

    void setEventFilter(vms::rules::EventFilter* eventFilter);
    vms::rules::EventFilter* eventFilter() const;

    virtual void setReadOnly(bool value);

signals:
    void actionBuilderChanged();
    void eventFilterChanged();
    void eventFieldsChanged();
    void actionFieldsChanged();

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
    vms::rules::ActionBuilder* m_actionBuilder{nullptr};
    vms::rules::EventFilter* m_eventFilter{nullptr};
    QHash<QString, vms::rules::Field*> m_eventFields;
    QHash<QString, vms::rules::Field*> m_actionFields;
};

} // namespace nx::vms::client::desktop::rules
