#pragma once

namespace nx {
namespace hpm {
namespace dao {
namespace rdb {

/**
 * #CLOUD-157. Gathering cloud connect session statistics.
 */
static const char kCreateConnectSessionStatisticsTable[] =
R"sql(

CREATE TABLE connect_session_statistics (
);

)sql";

} // namespace rdb
} // namespace dao
} // namespace hpm
} // namespace nx
