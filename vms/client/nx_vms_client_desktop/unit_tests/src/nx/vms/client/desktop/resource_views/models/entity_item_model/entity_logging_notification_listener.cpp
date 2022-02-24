// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_logging_notification_listener.h"

#include <algorithm>

#include <gtest/gtest.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {
namespace test {

//-------------------------------------------------------------------------------------------------
// NotificationRecord definitions.
//-------------------------------------------------------------------------------------------------

NotificationRecord::NotificationRecord()
{
}

NotificationRecord::NotificationRecord(NotificationType type):
    m_type(type)
{
}

QString NotificationRecord::toString() const
{
    auto entityToString =
        [](const AbstractEntity* item) -> QString
        {
            return QString::number(reinterpret_cast<unsigned long long>(item), 16)
                .toUpper()
                .prepend("0x");
        };

    auto indexesToString =
        [](const std::vector<int>& indexes) -> QString
        {
            QStringList stringIndexes;
            std::transform(std::cbegin(indexes), std::cend(indexes),
                std::back_inserter(stringIndexes),
                [](auto index) { return QString::number(index); });

            return stringIndexes.join(", ").prepend("[").append("]");
        };

    if (!m_type)
    {
        return "Error";
    }

    switch (*m_type)
    {
        case NotificationRecord::NotificationType::dataChanged:
            return QString("dataChanged(first:%1, last:%2)")
                .arg(m_indexes.at(0))
                .arg(m_indexes.at(1));

        case NotificationRecord::NotificationType::beginInsertRows:
            return QString("beginInsertRows(first:%1, last:%2)")
                .arg(m_indexes.at(0))
                .arg(m_indexes.at(1));

        case NotificationRecord::NotificationType::endInsertRows:
            return QString("endInsertRows()");

        case NotificationRecord::NotificationType::beginRemoveRows:
            return QString("beginRemoveRows(first:%1, last:%2)")
                .arg(m_indexes.at(0))
                .arg(m_indexes.at(1));

        case NotificationRecord::NotificationType::endRemoveRows:
            return QString("endRemoveRows()");

        case NotificationRecord::NotificationType::beginMoveRows:
            return QString(
                "beginMoveRows(Source: %1, First: %2, Last: %3, Destination: %4, Row: %5)")
                .arg(entityToString(m_source))
                .arg(m_indexes.at(0))
                .arg(m_indexes.at(1))
                .arg(entityToString(m_destination))
                .arg(m_indexes.at(2));

        case NotificationRecord::NotificationType::endMoveRows:
            return QString("endMoveRows()");
        case NotificationRecord::NotificationType::layoutAboutToBeChanged:
            return QString("layoutAboutToBeChanged(Permutation: %1)")
                .arg(indexesToString(m_indexes));
        case NotificationRecord::NotificationType::layoutChanged:
            return QString("layoutChanged()");
    }

    NX_ASSERT(false, "Unexpected state");
    return "Error";
}

bool NotificationRecord::operator==(const NotificationRecord& other) const
{
    return m_type == other.m_type
        && m_indexes == other.m_indexes;
}

NotificationRecord NotificationRecord::closingRecord(const NotificationRecord& other)
{
    if (!other.m_type)
        return nullRecord();

    switch (*other.m_type)
    {
        case NotificationType::beginInsertRows:
            return endInsertRows();
        case NotificationType::beginRemoveRows:
            return endRemoveRows();
        case NotificationType::beginMoveRows:
            return endMoveRows();
        case NotificationType::layoutAboutToBeChanged:
            return layoutChanged();
        default:
            return nullRecord();
    }
}

NotificationRecord NotificationRecord::dataChanged(
    int first, int last, const QVector<int>& roles)
{
    NotificationRecord result(NotificationType::dataChanged);
    result.m_indexes = {first, last};
    result.m_roles = {roles.cbegin(), roles.cend()};
    return result;
}

NotificationRecord NotificationRecord::beginInsertRows(int first, int last)
{
    NotificationRecord result(NotificationType::beginInsertRows);
    result.m_indexes = {first, last};
    return result;
}

NotificationRecord NotificationRecord::endInsertRows()
{
    return NotificationRecord(NotificationType::endInsertRows);
}

NotificationRecord NotificationRecord::beginRemoveRows(int first, int last)
{
    NotificationRecord result(NotificationType::beginRemoveRows);
    result.m_indexes = {first, last};
    return result;
}

NotificationRecord NotificationRecord::endRemoveRows()
{
    return NotificationRecord(NotificationType::endRemoveRows);
}

NotificationRecord NotificationRecord::beginMoveRows(
    const AbstractEntity* source, int first, int last,
    const AbstractEntity* desination, int row)
{
    NotificationRecord result(NotificationType::beginMoveRows);
    result.m_indexes = {first, last, row};
    result.m_source = source;
    result.m_destination = desination;
    return result;
}

NotificationRecord NotificationRecord::endMoveRows()
{
    return NotificationRecord(NotificationType::endMoveRows);
}

NotificationRecord NotificationRecord::layoutAboutToBeChanged(
    const std::vector<int>& indexPermutation)
{
    NotificationRecord result(NotificationType::layoutAboutToBeChanged);
    std::copy(std::cbegin(indexPermutation), std::cend(indexPermutation),
        std::back_inserter(result.m_indexes));
    return result;
}

NotificationRecord NotificationRecord::layoutChanged()
{
    NotificationRecord result(NotificationType::layoutChanged);
    return result;
}

NotificationRecord NotificationRecord::nullRecord()
{
    return NotificationRecord();
}

//-------------------------------------------------------------------------------------------------
// EntityLoggingNotificationListener definitions.
//-------------------------------------------------------------------------------------------------

void EntityLoggingNotificationListener::dataChanged(
    int first,
    int last,
    const QVector<int>& roles)
{
    m_records.push(NotificationRecord::dataChanged(first, last, roles));
}

void EntityLoggingNotificationListener::beginInsertRows(int first, int last)
{
    m_records.push(NotificationRecord::beginInsertRows(first, last));
}

void EntityLoggingNotificationListener::endInsertRows()
{
    m_records.push(NotificationRecord::endInsertRows());
}

void EntityLoggingNotificationListener::beginRemoveRows(int first, int last)
{
    m_records.push(NotificationRecord::beginRemoveRows(first, last));
}

void EntityLoggingNotificationListener::endRemoveRows()
{
    m_records.push(NotificationRecord::endRemoveRows());
}

void EntityLoggingNotificationListener::beginMoveRows(
    const AbstractEntity* source, int first, int last,
    const AbstractEntity* desination, int row)
{
    m_records.push(NotificationRecord::beginMoveRows(
        source, first, last, desination, row));
}

void EntityLoggingNotificationListener::endMoveRows()
{
    m_records.push(NotificationRecord::endMoveRows());
}

void EntityLoggingNotificationListener::layoutAboutToBeChanged(
    const std::vector<int>& indexPermutation)
{
    m_records.push(NotificationRecord::layoutAboutToBeChanged(indexPermutation));
}

void EntityLoggingNotificationListener::layoutChanged()
{
    m_records.push(NotificationRecord::layoutChanged());
}

int EntityLoggingNotificationListener::notificationQueueSize() const
{
    return m_records.size();
}

NotificationRecord EntityLoggingNotificationListener::popFront()
{
    if (m_records.empty())
        return NotificationRecord::nullRecord();
    NotificationRecord result = m_records.front();
    m_records.pop();
    return result;
}

bool EntityLoggingNotificationListener::empty() const
{
    return m_records.empty();
}

EntityNotificationQueueChecker EntityLoggingNotificationListener::queueChecker()
{
    return EntityNotificationQueueChecker(this);
}

std::ostream& operator<<(std::ostream& stream, const NotificationRecord& record)
{
    return stream << record.toString().toStdString();
}

//-------------------------------------------------------------------------------------------------
// EntityNotificationQueueChecker definitions.
//-------------------------------------------------------------------------------------------------

EntityNotificationQueueChecker::EntityNotificationQueueChecker(
    EntityLoggingNotificationListener* source)
    :
    m_source(source)
{
}

EntityNotificationQueueChecker& EntityNotificationQueueChecker::verifyNotification(
    const NotificationRecord& expectedRecord)
{
    if (m_source->empty())
    {
        m_outputTextLines.append(QString("Notification expected: %1, but queue is empty.")
            .arg(expectedRecord.toString()));

        failTest();
    }

    const auto frontRecord = m_source->popFront();
    const bool recordMatches = (frontRecord == expectedRecord);
    if (!recordMatches)
    {
        m_outputTextLines.append(QString("Expected notification: %1")
            .arg(expectedRecord.toString()));

        m_outputTextLines.append(QString("Actual notification..: %1")
            .arg(frontRecord.toString()));

        failTest();
    }
    m_lastProcessedRecord = frontRecord;

    return *this;
}

EntityNotificationQueueChecker& EntityNotificationQueueChecker::withClosing()
{
    if (!NX_ASSERT(m_lastProcessedRecord))
        failTest();

    const auto closingRecord = NotificationRecord::closingRecord(*m_lastProcessedRecord);
    if (closingRecord == NotificationRecord::nullRecord())
        failTest();

    return verifyNotification(NotificationRecord::closingRecord(*m_lastProcessedRecord));
}

void EntityNotificationQueueChecker::verifyIsEmpty()
{
    const bool isEmpty = m_source->empty();
    if (!isEmpty)
    {
        m_outputTextLines.append(
            QString("Recorded notification queue is not empty as expected, remaining record count is: %1")
            .arg(m_source->notificationQueueSize()));

        failTest();
    }
}

void EntityNotificationQueueChecker::failTest()
{
    GTEST_FAIL() << errorText();
}

std::string EntityNotificationQueueChecker::errorText()
{
    static constexpr int kLeftIndent = 13;
    static constexpr int kDelimiterWidth = 26;

    if (!m_annotation.isEmpty())
    {
        m_outputTextLines.prepend(QString(kDelimiterWidth, '-'));
        m_outputTextLines.prepend(m_annotation + " - FAIL");
    }

    for (auto& line: m_outputTextLines)
        line.prepend(QString(kLeftIndent, ' '));

    auto joinedText = m_outputTextLines.join("\n");
    joinedText.prepend("\n");
    joinedText.append("\n");

    m_outputTextLines.clear();

    return joinedText.toStdString();
}

EntityNotificationQueueChecker& EntityNotificationQueueChecker::withAnnotation(
    const QString& annotation)
{
    m_outputTextLines.clear();
    m_annotation = annotation;

    return *this;
}

} // namespace test
} // namespace entity_item_model
} // namespace nx::vms::client::desktop
