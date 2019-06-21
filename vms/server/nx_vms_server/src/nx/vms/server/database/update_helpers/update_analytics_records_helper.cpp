#include "update_analytics_records_helper.h"

#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/fusion/model_functions.h>

namespace nx::vms::server::database {

namespace {

static const QString kLogPrefix = "Updating the 'runtime_actions' table: ";

} // namespace

UpdateAnalyticsRecordsHelper::UpdateAnalyticsRecordsHelper(QSqlDatabase sqlDatabase):
    m_sdb(std::move(sqlDatabase))
{
}

bool UpdateAnalyticsRecordsHelper::doUpdate()
{
    if (!loadMapping())
        return false;

    if (!prepareUpdate())
        return false;

    return executeUpdate();
}

bool UpdateAnalyticsRecordsHelper::handleError(const QString& logMessage) const
{
    NX_ERROR(this, kLogPrefix + logMessage);
    return false;
}

bool UpdateAnalyticsRecordsHelper::loadMapping()
{
    QFile updateMappingFile(":/mserver_updates_data/16_update_analytics_event_records.json");
    if (!NX_ASSERT(updateMappingFile.open(QIODevice::ReadOnly)))
        return handleError("unable to open the event type GUID to Id mapping file");

    bool success = false;
    m_updateMapping = QJson::deserialized<std::map<QString, QString>>(
        updateMappingFile.readAll(),
        std::map<QString, QString>(),
        &success);

    if (!success)
        return handleError("unable to deserialize the event type GUID to Id mapping");

    if (m_updateMapping.empty())
        return handleError("the event type GUID to Id mapping is empty");

    return true;
}

bool UpdateAnalyticsRecordsHelper::prepareUpdate()
{
    QSqlQuery query(m_sdb);
    query.setForwardOnly(true);
    query.prepare(R"sql(
        SELECT
            rowid,
            action_params,
            runtime_params
        FROM runtime_actions
        WHERE event_type = :eventType;
    )sql");

    query.bindValue(":eventType", nx::vms::api::EventType::analyticsSdkEvent);

    if (!query.exec())
        return handleError("unable to select runtime actions");

    while (query.next())
    {
        const auto rowId = QnSql::deserialized_field<int>(query.value(0));
        const auto eventParameters =
            QnUbjson::deserialized<nx::vms::event::EventParameters>(query.value(2).toByteArray());

        QString newIdString;
        const auto oldGuidString = eventParameters.inputPortId;
        m_eventParametersByRowId[rowId] = eventParameters;

        if (const auto it = m_updateMapping.find(oldGuidString); it != m_updateMapping.cend())
        {
            newIdString = it->second;
        }
        else
        {
            // We consider all analytics events with an unknown event type id as events generated
            // by the Axis analytics plugin.
            newIdString = "nx.axis." + oldGuidString;
        }

        m_eventParametersByRowId[rowId].inputPortId = newIdString;
        NX_VERBOSE(this, kLogPrefix + "trying to replace %1 with %2", oldGuidString, newIdString);
    }

    return true;
}

bool UpdateAnalyticsRecordsHelper::executeUpdate()
{
    QSqlQuery updateQuery(m_sdb);
    updateQuery.prepare(R"sql(
        UPDATE runtime_actions
        SET runtime_params = :eventParameters, event_subtype = :eventSubtype
        WHERE rowid = :rowId;
    )sql");

    for (const auto& [rowId, eventParameters]: m_eventParametersByRowId)
    {
        updateQuery.bindValue(":eventParameters", QnUbjson::serialized(eventParameters));
        updateQuery.bindValue(":eventSubtype", QnUbjson::serialized(eventParameters.inputPortId));
        updateQuery.bindValue(":rowId", rowId);

        if (!updateQuery.exec())
        {
            return handleError(
                lm("unable to execute an update query with the following parameters: "
                    "eventParameters: %1, eventSubtype: %2, rowId: %3")
                    .args(QJson::serialized(eventParameters), eventParameters.inputPortId, rowId));
        }
    }

    return true;
}

} // namespace nx::vms::server::database
