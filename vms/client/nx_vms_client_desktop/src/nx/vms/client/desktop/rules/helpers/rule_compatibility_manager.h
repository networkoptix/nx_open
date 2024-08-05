// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/rules/rules_fwd.h>

namespace nx::vms::client::desktop::rules {

class RuleCompatibilityManager : public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    RuleCompatibilityManager(vms::rules::Rule* rule, SystemContext* context, QObject* parent = nullptr);

    void changeEventType(const QString& eventType);
    void changeActionType(const QString& actionType);

signals:
    // Emits when an event or action type or any field is changed.
    void ruleModified();

private:
    vms::rules::Rule* m_rule{};
    vms::rules::Engine* m_engine{};

    void onEventFilterFieldChanged(const QString& fieldName);
    void onActionBuilderFieldChanged(const QString& fieldName);

    void fixStateValue();
    void fixUseSourceValue();

    void onDurationChanged();
    void onAnalyticsEventTypeChanged();

    void replaceEventFilter(std::unique_ptr<vms::rules::EventFilter>&& eventFilter);
    void replaceActionBuilder(std::unique_ptr<vms::rules::ActionBuilder>&& actionBuilder);
};

} // namespace nx::vms::client::desktop::rules
