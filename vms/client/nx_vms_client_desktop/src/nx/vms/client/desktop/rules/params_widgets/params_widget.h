// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QVariant>
#include <QtWidgets/QWidget>

#include <nx/vms/rules/field.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/rule.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop::rules {

/**
 * Base class for the action and event parameters widgets. Represents a set of data pickers
 * according to the ItemDescriptor.
 */
class ParamsWidget: public QWidget, public WindowContextAware
{
    Q_OBJECT

public:
    explicit ParamsWidget(WindowContext* context, QWidget* parent = nullptr);

    void setRule(const std::shared_ptr<vms::rules::Rule>& rule);

    virtual std::optional<vms::rules::ItemDescriptor> descriptor() const = 0;
    std::optional<vms::rules::ItemDescriptor> actionDescriptor() const;
    vms::rules::ActionBuilder* actionBuilder() const;
    std::optional<vms::rules::ItemDescriptor> eventDescriptor() const;
    vms::rules::EventFilter* eventFilter() const;

    virtual void setReadOnly(bool value);

    virtual void updateUi() = 0;

protected:
    virtual void onRuleSet() = 0;

    /**
     * Returns field descriptor by the field name from the item descriptor set. If there is
     * no such a field in the item descriptor returns nullopt.
     */
    std::optional<vms::rules::FieldDescriptor> fieldDescriptor(const QString& fieldName) const;

private:
    void setupLineEditsPlaceholderColor();

    std::shared_ptr<vms::rules::Rule> m_rule;
};

} // namespace nx::vms::client::desktop::rules
