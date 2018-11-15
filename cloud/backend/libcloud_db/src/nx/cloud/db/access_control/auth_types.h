#pragma once

#include <nx/utils/stree/resourcecontainer.h>

namespace nx::cloud::db {

enum class AccessRole
{
    none,
    /** System, binded to account can fetch some data too. */
    system,
    user,
    /** Other nx_cloud module. */
    nxCloudModule,
    /**
     * Used by classes of this module to communicate to each other.
     * Everything is allowed for this role.
     */
    cloudDB,
};

enum class AuthenticationType
{
    credentials,
    other,
};

/**
 * Contains information about authenticated user.
 * NOTE: This information plus requested action type is enough to perform authorization.
 */
class AuthenticationInfo
{
public:
    AccessRole role;
    /**
     * Parameters, discovered during authentication (e.g., account id).
     */
    nx::utils::stree::ResourceContainer params;
};

class AuthorizationInfo:
    public nx::utils::stree::AbstractResourceReader,
    public nx::utils::stree::AbstractIteratableContainer
{
public:
    AuthorizationInfo() = default;
    AuthorizationInfo(nx::utils::stree::ResourceContainer rc);

    virtual bool getAsVariant(int resID, QVariant* const value) const override;
    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const override;

    bool accessAllowedToOwnDataOnly() const;

private:
    nx::utils::stree::ResourceContainer m_rc;
};

} // namespace nx::cloud::db
