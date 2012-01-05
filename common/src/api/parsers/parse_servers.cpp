#include "api/parsers/parse_servers.h"

#include "core/resourcemanagment/asynch_seacher.h"

QnApiServerPtr unparseServer(const QnVideoServer& serverIn)
{
    return QnApiServerPtr(
                new xsd::api::servers::Server(
                   serverIn.getId().toString().toStdString(),
                   serverIn.getName().toStdString(),
                   serverIn.getTypeId().toString().toStdString(),
                   serverIn.getUrl().toStdString(),
                   serverIn.getGuid().toStdString(),
                   serverIn.getApiUrl().toStdString()));
}
