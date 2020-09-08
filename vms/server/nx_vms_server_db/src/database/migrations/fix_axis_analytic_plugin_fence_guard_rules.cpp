#include "fix_axis_analytic_plugin_fence_guard_rules.h"

#include <optional>
#include <map>

#include <nx/fusion/model_functions.h>
#include <nx/utils/uuid.h>
#include <nx/utils/log/log_message.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/sql/sql_query_execution_helper.h>

#include <QtCore/QString>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

namespace ec2::db {

namespace {

QString makeTypeId(int channelIndex, std::optional<int> profileIndex = std::nullopt)
{
    const auto rawId = NX_FMT("CameraApplicationPlatform/FenceGuard/Camera%1Profile%2",
        channelIndex, profileIndex ? QString::number(*profileIndex) : "ANY");

    return NX_FMT("nx.axis.%1", QnUuid::fromArbitraryData(rawId));
}

std::optional<std::map<QString, QString>> getEventTypeNames(QSqlDatabase& db)
{
    constexpr int kMaxChannelIndex = 4;
    constexpr int kMaxProfileIndex = 32;

    std::map<QString, QString> typeNames;
    for (int channelIndex = 1; channelIndex <= kMaxChannelIndex; ++channelIndex)
    {
        for (int profileIndex = 1; profileIndex <= kMaxProfileIndex; ++profileIndex)
            typeNames.emplace(makeTypeId(channelIndex, profileIndex), "");
    }

    QSqlQuery query(db);
    query.setForwardOnly(true);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query,
            "SELECT value FROM vms_kvpair WHERE name = \"eventTypeDescriptors\"", Q_FUNC_INFO) ||
        !nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
    {
        return std::nullopt;
    }

    while (query.next())
    {
        std::map<QString, nx::vms::api::analytics::EventTypeDescriptor> descriptors;
        if (!QJson::deserialize(query.value(0).toByteArray(), &descriptors))
            continue;

        for (const auto& [id, descriptor]: descriptors)
        {
            if (const auto it = typeNames.find(id); it != typeNames.end())
            {
                auto& name = it->second;
                name = descriptor.name;

                static const QString kPrefix = "Fence Guard: ";
                if (NX_ASSERT(name.startsWith(kPrefix)))
                    name = name.mid(kPrefix.length());
            }
        }
    }

    for (int channelIndex = 1; channelIndex <= kMaxChannelIndex; ++channelIndex)
        typeNames.emplace(makeTypeId(channelIndex), "");

    return typeNames;
}

} // namespace

bool fixAxisAnalyticPluginFenceGuardRules(QSqlDatabase& db)
{
    const auto typeNames = getEventTypeNames(db);
    if (!typeNames)
        return false;

    QSqlQuery update(db);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&update,
            "UPDATE vms_businessrule SET event_condition = ? WHERE rowid = ?", Q_FUNC_INFO))
    {
        return false;
    }

    QSqlQuery select(db);
    select.setForwardOnly(true);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&select,
            "SELECT rowid, event_condition FROM vms_businessrule", Q_FUNC_INFO) ||
        !nx::sql::SqlQueryExecutionHelper::execSQLQuery(&select, Q_FUNC_INFO))
    {
        return false;
    }

    while (select.next())
    {
        int rowid = select.value(0).toInt();

        nx::vms::event::EventParameters params;
        if (!QJson::deserialize(select.value(1).toByteArray(), &params))
            continue;

        if (const auto it = typeNames->find(params.inputPortId); it != typeNames->end())
        {
            params.inputPortId = "nx.axis.FenceGuard";

            if (const auto& name = it->second; !name.isEmpty())
            {
                if (params.description.isEmpty())
                    params.description = name;
                else
                    params.description += " " + name;
            }

            update.addBindValue(QJson::serialized(params));
            update.addBindValue(rowid);

            if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&update, Q_FUNC_INFO))
                return false;
        }
    }

    return true;
}

} // namespace ec2::db
