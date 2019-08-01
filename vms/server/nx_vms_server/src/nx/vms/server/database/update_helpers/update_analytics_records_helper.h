#pragma once

#include <map>

#include <QtSql/QSqlDatabase>

#include <nx/vms/event/event_parameters.h>

namespace nx::vms::server::database {

class UpdateAnalyticsRecordsHelper
{
public:
    UpdateAnalyticsRecordsHelper(QSqlDatabase sqlDatabase);

    bool doUpdate();

private:
    bool handleError(const QString& logMessage) const;

    bool loadMapping();

    bool prepareUpdate();

    bool executeUpdate();

private:
    QSqlDatabase m_sdb;
    std::map<QString, QString> m_updateMapping;
    std::map<int, nx::vms::event::EventParameters> m_eventParametersByRowId;
};

} // namespace nx::vms::server::database
