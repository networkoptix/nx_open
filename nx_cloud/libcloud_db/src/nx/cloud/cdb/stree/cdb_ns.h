#pragma once

#include <nx/utils/stree/resourcenameset.h>

namespace nx {
namespace cdb {

/**
 * Attributes to be used with stree.
 */
namespace attr {

enum Value
{
    operation = 1,

    //source data attributes
    accountId,
    accountEmail,
    ha1,
    userName,
    userPassword,
    accountStatus,
    systemAccessRole,

    systemId,
    systemStatus,   /** int */
    customization,

    dataAccountRightsOnSystem,

    //authentication/authorization intermediate attributes
    authenticated,
    authorized,
    authAccountRightsOnSystem,
    authAccountEmail,
    authSystemId,
    /** Operation data contains accountId equal to the one being authenticated. */
    authSelfAccountAccessRequested,
    /** Operation data contains systemId equal to the one being authenticated. */
    authSelfSystemAccessRequested,
    /** Request has been authenticated by code sent to account email. */
    authenticatedByEmailCode,
    resultCode,

    entity,
    action,
    secureSource,

    socketIntfIP,
    socketRemoteIP,

    requestPath,

    credentialsType = 100,
    credentialsExpirationPeriod,
    credentialsProlongationPeriod
};

} // namespace attr

/**
 * Contains description of stree attributes used by cloud_db.
 */
class CdbAttrNameSet:
    public nx::utils::stree::ResourceNameSet
{
public:
    CdbAttrNameSet();
};

} // namespace cdb
} // namespace nx
