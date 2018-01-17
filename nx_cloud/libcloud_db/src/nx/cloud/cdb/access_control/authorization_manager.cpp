#include "authorization_manager.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>
#include <nx/cloud/cdb/client/data/types.h>

#include "../managers/account_manager.h"
#include "../managers/system_manager.h"
#include "../stree/cdb_ns.h"
#include "../stree/stree_manager.h"

namespace nx {
namespace cdb {

AuthorizationManager::AuthorizationManager(
    const StreeManager& stree,
    const AbstractAccountManager& accountManager,
    const AbstractSystemManager& systemManager,
    const AbstractSystemSharingManager& systemSharingManager)
:
    m_stree(stree),
    m_accountManager(accountManager),
    m_systemManager(systemManager),
    m_systemSharingManager(systemSharingManager)
{
}

bool AuthorizationManager::authorize(
    const nx::utils::stree::AbstractResourceReader& authenticationProperties,
    const nx::utils::stree::AbstractResourceReader& dataToAuthorize,
    EntityType requestedEntity,
    DataActionType requestedAction,
    nx::utils::stree::AbstractResourceWriter* const authzInfo) const
{
    // Adding helper attributes.
    nx::utils::stree::ResourceContainer auxSearchAttrs =
        addHelperAttributes(
            authenticationProperties,
            dataToAuthorize);

    // Forwarding requestedEntity and requestedAction.
    auxSearchAttrs.put(attr::entity, QnLexical::serialized(requestedEntity));
    auxSearchAttrs.put(attr::action, QnLexical::serialized(requestedAction));

    nx::utils::stree::MultiSourceResourceReader inputData(
        authenticationProperties,
        dataToAuthorize,
        auxSearchAttrs);

    if (!checkStaticRules(requestedEntity, requestedAction, inputData, authzInfo))
        return false;

    return checkDynamicRules(inputData, authzInfo);
}

nx::utils::stree::ResourceContainer AuthorizationManager::addHelperAttributes(
    const nx::utils::stree::AbstractResourceReader& authenticationProperties,
    const nx::utils::stree::AbstractResourceReader& dataToAuthorize) const
{
    nx::utils::stree::ResourceContainer auxSearchAttrs;

    // Adding authAccountRightsOnSystem if appropriate.
    const auto authenticatedAccountEmail =
        authenticationProperties.get<std::string>(attr::authAccountEmail);
    const auto authenticatedSystemId =
        authenticationProperties.get<std::string>(attr::authSystemId);
    if (authenticatedSystemId)
        auxSearchAttrs.put(attr::systemId, authenticatedSystemId->c_str());

    const auto requestedSystemId = dataToAuthorize.get<std::string>(attr::systemId);
    if (authenticatedAccountEmail)
    {
        // Adding account status.
        if (auto account = m_accountManager.findAccountByUserName(authenticatedAccountEmail.get()))
            auxSearchAttrs.put(attr::accountStatus, static_cast<int>(account->statusCode));

        if (requestedSystemId)
        {
            // Account requests access to the system.
            auxSearchAttrs.put(
                attr::authAccountRightsOnSystem,
                QnLexical::serialized(
                    m_systemSharingManager.getAccountRightsForSystem(
                        authenticatedAccountEmail.get(),
                        requestedSystemId.get())));
        }
    }

    const auto requestedAccountEmail = dataToAuthorize.get<std::string>(attr::accountEmail);
    if (requestedAccountEmail && requestedSystemId)
    {
        auxSearchAttrs.put(
            attr::dataAccountRightsOnSystem,
            QnLexical::serialized(
                m_systemSharingManager.getAccountRightsForSystem(
                    requestedAccountEmail.get(),
                    requestedSystemId.get())));
    }

    if (authenticatedAccountEmail && requestedAccountEmail &&
        authenticatedAccountEmail.get() == requestedAccountEmail.get())
    {
        auxSearchAttrs.put(attr::authSelfAccountAccessRequested, true);
    }

    if (authenticatedSystemId && requestedSystemId &&
        *authenticatedSystemId == requestedSystemId)
    {
        auxSearchAttrs.put(attr::authSelfSystemAccessRequested, true);
    }

    return auxSearchAttrs;
}

bool AuthorizationManager::checkStaticRules(
    EntityType requestedEntity,
    DataActionType requestedAction,
    const nx::utils::stree::AbstractResourceReader& inputData,
    nx::utils::stree::AbstractResourceWriter* outAuthzInfo) const
{
    NX_VERBOSE(this, "Checking static authorization rules");

    if (requestedEntity == EntityType::system &&
        (requestedAction == DataActionType::update || requestedAction == DataActionType::delete_))
    {
        // TODO: #ak Introduce some general code here when adding next static rule.

        // Checking system state.
        const auto systemId = inputData.get<std::string>(attr::systemId);
        if (!systemId)
            return false;
        const auto system = m_systemManager.findSystemById(*systemId);
        if (!system)
            return false;
        if (system->status == api::SystemStatus::beingMerged)
        {
            outAuthzInfo->put(
                attr::resultCode,
                QnLexical::serialized(api::ResultCode::notAllowedInCurrentState));
            return false;
        }
    }

    return true;
}

bool AuthorizationManager::checkDynamicRules(
    const nx::utils::stree::AbstractResourceReader& inputData,
    nx::utils::stree::AbstractResourceWriter* outAuthzInfo) const
{
    NX_VERBOSE(this, "Checking dynamic authorization rules");

    nx::utils::stree::ResourceContainer additionalResourceContainer;
    nx::utils::stree::ResourceWriterProxy resProxy(outAuthzInfo, &additionalResourceContainer);
    m_stree.search(
        StreeOperation::authorization,
        nx::utils::stree::MultiSourceResourceReader(
            inputData,
            additionalResourceContainer),
        &resProxy);

    if (auto authorized = additionalResourceContainer.get<bool>(attr::authorized))
        return authorized.get();

    // No "authorized" attr has been set. This is actually error in authorization rules xml.
    NX_ASSERT(false);
    NX_LOG(lit("No \"authorized\" attribute has been set by authorization tree traversal"), cl_logWARNING);
    return false;
}

} // namespace cdb
} // namespace nx
