#include "cdb_ns.h"

namespace nx {
namespace cdb {

CdbAttrNameSet::CdbAttrNameSet()
{
    registerResource(attr::operation, "operation", QVariant::String);
    registerResource(attr::accountId, "accountId", QVariant::String);
    registerResource(attr::accountEmail, "accountEmail", QVariant::String);
    registerResource(attr::ha1, "ha1", QVariant::String);
    registerResource(attr::userName, "user.name", QVariant::String);
    registerResource(attr::userPassword, "user.password", QVariant::String);
    registerResource(attr::accountStatus, "account.status", QVariant::Int);
    registerResource(attr::systemAccessRole, "systemAccessRole", QVariant::String);

    registerResource(attr::systemId, "systemId", QVariant::String);
    registerResource(attr::systemStatus, "systemStatus", QVariant::Int);
    registerResource(attr::customization, "customization", QVariant::String);

    registerResource(attr::dataAccountRightsOnSystem, "data.accountRightsOnSystem", QVariant::String);

    registerResource(attr::authenticated, "authenticated", QVariant::Bool);
    registerResource(attr::authorized, "authorized", QVariant::Bool);
    registerResource(attr::authAccountRightsOnSystem, "auth.accountRightsOnSystem", QVariant::String);
    registerResource(attr::authAccountEmail, "auth.accountEmail", QVariant::String);
    registerResource(attr::authSystemId, "auth.systemId", QVariant::String);
    registerResource(attr::authSelfAccountAccessRequested, "auth.selfAccountAccessRequested", QVariant::Bool);
    registerResource(attr::authSelfSystemAccessRequested, "auth.selfSystemAccessRequested", QVariant::Bool);
    registerResource(attr::authenticatedByEmailCode, "authenticatedByEmailCode", QVariant::Bool);
    registerResource(attr::resultCode, "resultCode", QVariant::String);

    registerResource(attr::entity, "entity", QVariant::String);
    registerResource(attr::action, "action", QVariant::String);
    registerResource(attr::secureSource, "secureSource", QVariant::Bool);

    registerResource(attr::socketIntfIP, "socket.intf.ip", QVariant::String);
    registerResource(attr::socketRemoteIP, "socket.remoteIP", QVariant::String);

    registerResource(attr::requestPath, "request.path", QVariant::String);

    registerResource(attr::credentialsType, "credentials.type", QVariant::String);
    registerResource(attr::credentialsExpirationPeriod, "credentials.expirationPeriod", QVariant::String);
    registerResource(attr::credentialsProlongationPeriod, "credentials.prolongationPeriod", QVariant::String);
}

} // namespace cdb
} // namespace nx
