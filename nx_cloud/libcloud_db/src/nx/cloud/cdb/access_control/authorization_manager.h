#pragma once

#include <nx/utils/stree/resourcecontainer.h>

#include "../managers/managers_types.h"
#include "auth_types.h"

namespace nx {
namespace cdb {

class AbstractAccountManager;
class StreeManager;
class AbstractSystemManager;
class AbstractSystemSharingManager;

/**
 * Performs request authorization based on authentication info,
 * requested entity and requested action type.
 * Checks two types of authorization rules:
 * - static. Defined by calls to this class.
 *   These are rules that are required for other parts of code to function correctly.
 * - dynamic. Defined by authorization_rules.xml.
 *   These rules can be safely altered (e.g., for customization purposes) in xml without altering code.
 */
class AuthorizationManager
{
public:
    AuthorizationManager(
        const StreeManager& stree,
        const AbstractAccountManager& accountManager,
        const AbstractSystemManager& systemManager,
        const AbstractSystemSharingManager& systemSharingManager);

    /**
     * @param dataToAuthorize Authorization information can
     *   contain data filter to restrict access to data.
     * @return true if authorized, false otherwise.
     * NOTE: This method cannot block.
     */
    bool authorize(
        const nx::utils::stree::AbstractResourceReader& authenticationProperties,
        const nx::utils::stree::AbstractResourceReader& dataToAuthorize,
        EntityType requestedEntity,
        DataActionType requestedAction,
        nx::utils::stree::AbstractResourceWriter* const authzInfo) const;

private:
    const StreeManager& m_stree;
    const AbstractAccountManager& m_accountManager;
    const AbstractSystemManager& m_systemManager;
    const AbstractSystemSharingManager& m_systemSharingManager;

    nx::utils::stree::ResourceContainer addHelperAttributes(
        const nx::utils::stree::AbstractResourceReader& authenticationProperties,
        const nx::utils::stree::AbstractResourceReader& dataToAuthorize) const;

    bool checkStaticRules(
        EntityType requestedEntity,
        DataActionType requestedAction,
        const nx::utils::stree::AbstractResourceReader& inputData,
        nx::utils::stree::AbstractResourceWriter* outAuthzInfo) const;

    bool checkDynamicRules(
        const nx::utils::stree::AbstractResourceReader& inputData,
        nx::utils::stree::AbstractResourceWriter* const outAuthzInfo) const;
};

} // namespace cdb
} // namespace nx
