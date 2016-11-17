#pragma once

#include <QtSql/QSqlDatabase>

#include <nx_ec/data/api_fwd.h>

namespace ec2 {
namespace database {
namespace api {

/** Get id from vms_resource table. Returns 0 if resource is not found. */
qint32 getResourceInternalId(
    const QSqlDatabase& database,
    const QnUuid& guid);

/** Add or update resource. */
bool insertOrReplaceResource(
    const QSqlDatabase& database,
    const ApiResourceData& data,
    qint32* internalId);

} // namespace api
} // namespace database
} // namespace ec2
