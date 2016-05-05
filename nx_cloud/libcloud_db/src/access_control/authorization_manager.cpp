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
    //adding helper attributes

    //adding authAccountRightsOnSystem if appropriate
    const auto authenticatedAccountEmail = 
        authenticationProperties.get<std::string>(attr::authAccountEmail);
    const auto authenticatedSystemID =
        authenticationProperties.get<std::string>(attr::authSystemID);
    const auto requestedSystemID = dataToAuthorize.get<std::string>(attr::systemID);
    stree::ResourceContainer auxSearchAttrs;
    if (authenticatedAccountEmail)
    {
        //adding account status
        if (auto account = m_accountManager.findAccountByUserName(authenticatedAccountEmail.get()))
            auxSearchAttrs.put(attr::accountStatus, static_cast<int>(account->statusCode));

        if (requestedSystemID)
        {
            //account requests access to the system
            auxSearchAttrs.put(
                attr::authAccountRightsOnSystem,
                QnLexical::serialized(
                    m_systemManager.getAccountRightsForSystem(
                        authenticatedAccountEmail.get(),
                        requestedSystemID.get())));
        }
    }

    const auto requestedAccountEmail = dataToAuthorize.get<std::string>(attr::accountEmail);
    if (requestedAccountEmail && requestedSystemID)
    {
        auxSearchAttrs.put(
            attr::dataAccountRightsOnSystem,
            QnLexical::serialized(
                m_systemManager.getAccountRightsForSystem(
                    requestedAccountEmail.get(),
                    requestedSystemID.get())));
    }

    if (authenticatedAccountEmail && requestedAccountEmail &&
        authenticatedAccountEmail.get() == requestedAccountEmail.get())
    {
        auxSearchAttrs.put(
            attr::authSelfAccountAccessRequested,
            true);
    }

    if (authenticatedSystemID && requestedSystemID &&
        *authenticatedSystemID == requestedSystemID)
    {
        auxSearchAttrs.put(
            attr::authSelfSystemAccessRequested,
            true);
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
    NX_ASSERT(false);
    NX_LOG(lit("No \"authorized\" attribute has been set by authorization tree traversal"), cl_logWARNING);
    return false;
}

}   //cdb
}   //nx
