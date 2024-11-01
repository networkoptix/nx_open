// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context_aware.h>

#include "../rules_fwd.h"

namespace nx::vms::rules::utils {

class NX_VMS_RULES_API CompatibilityManager : public QObject, public common::SystemContextAware
{
    Q_OBJECT

public:
    CompatibilityManager(
        vms::rules::Rule* rule, common::SystemContext* context, QObject* parent = nullptr);

    void changeEventType(const QString& eventType);
    void changeActionType(const QString& actionType);

    // This method is introduced for test purposes only.
    void setDefaultActionType(const QString& type);

signals:
    // Emits when an event or action type or any field is changed.
    void ruleModified();

private:
    vms::rules::Rule* m_rule{};
    vms::rules::Engine* m_engine{};

    std::optional<UuidSet> m_lastEventCamerasSelection;
    std::optional<UuidSet> m_lastActionCamerasSelection;

    QString m_defaultActionType;

    void onEventFilterFieldChanged(const QString& fieldName);
    void onActionBuilderFieldChanged(const QString& fieldName);

    void fixDurationValue();
    void fixStateValue();
    void fixUseSourceValue();

    void onDurationChanged();
    void onAnalyticsEventTypeChanged();

    void replaceEventFilter(std::unique_ptr<vms::rules::EventFilter>&& eventFilter);
    void replaceActionBuilder(std::unique_ptr<vms::rules::ActionBuilder>&& actionBuilder);
    void checkCompatibility();
};

} // namespace nx::vms::rules::utils
