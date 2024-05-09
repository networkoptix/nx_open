#include "abstract_db_connection.h"

#include "db_statistics_collector.h"

namespace nx::sql {

void AbstractDbConnection::setStatisticsCollector(StatisticsCollector* statisticsCollector)
{
	m_statisticsCollector = statisticsCollector;
}

StatisticsCollector* AbstractDbConnection::statisticsCollector()
{
	return m_statisticsCollector;
}

} // namespace nx::sql
