// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
