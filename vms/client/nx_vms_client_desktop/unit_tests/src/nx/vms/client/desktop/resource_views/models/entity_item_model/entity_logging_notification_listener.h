// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>
#include <iostream>
#include <optional>

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

//-------------------------------------------------------------------------------------------------
// NotificationRecord declaration.
//-------------------------------------------------------------------------------------------------

class NotificationRecord
{
public:
    // Data items, each represent notification log entry.
    static NotificationRecord dataChanged(int first, int last, const QVector<int>& roles);
    static NotificationRecord beginInsertRows(int first, int last);
    static NotificationRecord endInsertRows();
    static NotificationRecord beginRemoveRows(int first, int last);
    static NotificationRecord endRemoveRows();
    static NotificationRecord beginMoveRows(
        const AbstractEntity* source, int first, int last,
        const AbstractEntity* desination, int row);
    static NotificationRecord endMoveRows();
    static NotificationRecord layoutAboutToBeChanged(const std::vector<int>& indexPermutation);
    static NotificationRecord layoutChanged();

    // Null item, may be acquired by querying empty log storage with some expectations. Thus,
    // getting one is generally sign of fault.
    static NotificationRecord nullRecord();

    static NotificationRecord closingRecord(const NotificationRecord& other);

    QString toString() const;
    bool operator==(const NotificationRecord& other) const;

private:
    enum class NotificationType
    {
        dataChanged,
        beginInsertRows,
        endInsertRows,
        beginRemoveRows,
        endRemoveRows,
        beginMoveRows,
        endMoveRows,
        layoutAboutToBeChanged,
        layoutChanged,
    };
    NotificationRecord();
    NotificationRecord(NotificationType type);

private:
    std::optional<NotificationType> m_type;
    std::vector<int> m_indexes;
    std::vector<int> m_roles;
    const AbstractEntity* m_source = nullptr;
    const AbstractEntity* m_destination = nullptr;
};

std::ostream& operator<<(std::ostream& stream, const NotificationRecord& record);

//-------------------------------------------------------------------------------------------------
// EntityNotificationQueueChecker declaration.
//-------------------------------------------------------------------------------------------------

class EntityLoggingNotificationListener;

// Helper class provided by EntityLoggingNotificationListener. Used for make assertions on logged
// entity notifications data. Please keep in mind that verifyNotification() call clears logger
// FIFO queue.
class EntityNotificationQueueChecker
{
public:
    EntityNotificationQueueChecker(EntityLoggingNotificationListener* source);

    // Checks nothing, but stores any text which will be displayed in case if fail. Relevant only
    // For subsequent verify*** calls. Each call discards previous. Useful for processing long
    // logs block by block.
    EntityNotificationQueueChecker& withAnnotation(const QString& annotation);

    // Takes recorded notification in FIFO order from source and compares with given parameters.
    // If case of the unexpected result generates failed test state and provides human readable
    // output. Returns reference to self for convenient sequential cheeking.
    EntityNotificationQueueChecker& verifyNotification(const NotificationRecord& expectedRecord);

    EntityNotificationQueueChecker& withClosing();

    // Assertion claiming that all recorded notifications has been processed. If not, generates
    // failed test state, and outputs rest of the records.
    void verifyIsEmpty();

private:
    void failTest();
    std::string errorText();

    std::optional<NotificationRecord> m_lastProcessedRecord;
    EntityLoggingNotificationListener* m_source;
    QStringList m_outputTextLines;
    QString m_annotation;
};

//-------------------------------------------------------------------------------------------------
// EntityLoggingNotificationListener declaration.
//-------------------------------------------------------------------------------------------------

class EntityLoggingNotificationListener: public BaseNotificatonObserver
{
public:
    bool empty() const;
    int notificationQueueSize() const;

    NotificationRecord popFront();

    EntityNotificationQueueChecker queueChecker();

public:
    virtual void dataChanged(int first, int last, const QVector<int>& roles) override;

    virtual void beginInsertRows(int first, int last) override;
    virtual void endInsertRows() override;

    virtual void beginRemoveRows(int first, int last) override;
    virtual void endRemoveRows() override;

    virtual void beginMoveRows(
        const AbstractEntity* source, int first, int last,
        const AbstractEntity* desination, int row) override;

    virtual void endMoveRows() override;

    virtual void layoutAboutToBeChanged(const std::vector<int>& indexPermutation) override;
    virtual void layoutChanged() override;

private:
    std::queue<NotificationRecord> m_records;
};

} // namespace test
} // namespace entity_item_model
} // namespace nx::vms::client::desktop
