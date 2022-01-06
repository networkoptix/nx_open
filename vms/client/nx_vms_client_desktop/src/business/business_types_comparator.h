// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/event/event_fwd.h>

// TODO: #vkutin Get rid of 'business' and put this class to proper namespace.

/** Helper class to sort business types lexicographically. */
class QnBusinessTypesComparator: public QObject, public nx::vms::client::core::CommonModuleAware
{
    using base_type = QObject;
public:
    explicit QnBusinessTypesComparator(QObject* parent = nullptr);

    bool lexicographicalLessThan(nx::vms::api::EventType left, nx::vms::api::EventType right) const;
    bool lexicographicalLessThan(nx::vms::api::ActionType left, nx::vms::api::ActionType right) const;

    /*
     * Reorder event types to lexicographical order (for sorting)
     */
    int toLexEventType(nx::vms::api::EventType eventType) const;

    /*
     * Reorder actions types to lexicographical order (for sorting)
     */
    int toLexActionType(nx::vms::api::ActionType actionType) const;

    QList<nx::vms::api::EventType> lexSortedEvents(
        const QList<nx::vms::api::EventType>& events) const;

    QList<nx::vms::api::ActionType> lexSortedActions(
        const QList<nx::vms::api::ActionType>& actions) const;

private:
    void initLexOrdering();

private:
    QVector<int> m_eventTypeToLexOrder;
    QVector<int> m_actionTypeToLexOrder;
};
