#pragma once

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

/**
 * #CLOUD-157. Gathering cloud connect session statistics.
 */
static const char kCreateConnectSessionStatisticsTable[] =
R"sql(

CREATE TABLE connect_session_statistics (
    id                          %bigint_primary_key_auto_increment%,
    start_time_utc              INTEGER,
    end_time_utc                INTEGER,
    result_code                 INTEGER,
    session_id                  VARCHAR(64),
    originating_host_endpoint   VARCHAR(256),
    originating_host_name       VARCHAR(256),
    destination_host_endpoint   VARCHAR(256),
    destination_host_name       VARCHAR(256)
);

)sql";

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
