#pragma once

#include <business/business_fwd.h>

/** Helper class to sort business types lexicographically. */
class QnBusinessTypesComparator: public QObject {
public:
    QnBusinessTypesComparator(QObject *parent = nullptr);

    bool lexicographicalLessThan(QnBusiness::EventType left, QnBusiness::EventType right) const;
    bool lexicographicalLessThan(QnBusiness::ActionType left, QnBusiness::ActionType right) const;

    /*
     * Reorder event types to lexicographical order (for sorting)
     */
    int toLexEventType(QnBusiness::EventType eventType) const;

    /*
     * Reorder actions types to lexicographical order (for sorting)
     */
    int toLexActionType(QnBusiness::ActionType actionType) const;

private:
    void initLexOrdering();

private:
    QVector<int> m_eventTypeToLexOrder;
    QVector<int> m_actionTypeToLexOrder;
};
