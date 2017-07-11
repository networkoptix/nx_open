#pragma once

#include <QtSql/QSqlDatabase>

#include <nx_ec/data/api_fwd.h>
#include "db_resource_api.h"

namespace ec2 {
namespace database {
namespace api {

/**
* Add or update web page.
* @param in database Target database.
* @param in layout Web page api data.
* @returns True if operation was successful, false otherwise.
*/
bool saveWebPage(ec2::database::api::QueryContext* resourceContext, const ApiWebPageData& webPage);

} // namespace api
} // namespace database
} // namespace ec2
