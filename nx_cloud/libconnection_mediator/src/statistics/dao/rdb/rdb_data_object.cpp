#include "rdb_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

nx::db::DBResult RdbDataObject::save(
    nx::db::QueryContext* /*queryContext*/,
    ConnectionRecord /*connectionRecord*/)
{
    // TODO:
    return nx::db::DBResult::ok;
}

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
