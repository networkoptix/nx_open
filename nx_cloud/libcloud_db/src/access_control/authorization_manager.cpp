/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#include "authorization_manager.h"

#include <utils/serialization/lexical.h>

#include "managers/account_manager.h"
#include "managers/system_manager.h"
#include "stree/cdb_ns.h"
#include "stree/stree_manager.h"


namespace nx {
namespace cdb {

AuthorizationManager::AuthorizationManager(
    const StreeManager& stree,
    const AccountManager& accountManager,
    const SystemManager& systemManager)
:
    m_stree(stree),
    m_accountManager(accountManager),
    m_systemManager(systemManager)
{
}

bool AuthorizationManager::authorize(
    const stree::AbstractResourceReader& authenticationProperties,
    const stree::AbstractResourceReader& dataToAuthorize,
    EntityType requestedEntity,
    DataActionType requestedAction,
    stree::AbstractResourceWriter* const authzInfo ) const
{
    //adding authAccountRightsOnSystem if appropriate
    const auto authenticatedAccountID = authenticationProperties.get<QnUuid>(attr::authAccountID);
    const auto requestedSystemID = dataToAuthorize.get<QnUuid>(attr::systemID);
    stree::ResourceContainer auxSearchAttrs;
    if (authenticatedAccountID)
    {
        //adding account status
        if (auto account = m_accountManager.findAccountByID(authenticatedAccountID.get()))
            auxSearchAttrs.put(attr::accountStatus, static_cast<int>(account->statusCode));

        if (requestedSystemID)
        {
            //account requests access to the system
            auxSearchAttrs.put(
                attr::authAccountRightsOnSystem,
                QnLexical::serialized(
                    m_systemManager.getAccountRightsForSystem(
                        authenticatedAccountID.get(),
                        requestedSystemID.get())));
        }
    }

    const auto requestedAccountID = dataToAuthorize.get<QnUuid>(attr::accountID);
    if (requestedAccountID && requestedSystemID)
    {
        auxSearchAttrs.put(
            attr::dataAccountRightsOnSystem,
            QnLexical::serialized(
                m_systemManager.getAccountRightsForSystem(
                    requestedAccountID.get(),
                    requestedSystemID.get())));
    }

    //forwarding requestedEntity and requestedAction
    auxSearchAttrs.put(attr::entity, QnLexical::serialized(requestedEntity));
    auxSearchAttrs.put(attr::action, QnLexical::serialized(requestedAction));

    stree::ResourceWriterProxy<bool> resProxy(authzInfo, attr::authorized);
    m_stree.search(
        StreeOperation::authorization,
        stree::MultiSourceResourceReader(
            authenticationProperties,
            dataToAuthorize,
            auxSearchAttrs),
        &resProxy);

    if (auto authorized = resProxy.take())
        return authorized.get();
    //no "autorized" attr has been set. This is actually error in authorization rules xml
    Q_ASSERT(false);
    NX_LOG(lit("No \"authorized\" attribute has been set by authorization tree traversal"), cl_logWARNING);
    return false;
}

}   //cdb
}   //nx
