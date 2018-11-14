#include "auth_types.h"

namespace nx::cloud::db {

AuthorizationInfo::AuthorizationInfo(nx::utils::stree::ResourceContainer rc):
    m_rc(std::move(rc))
{
}

bool AuthorizationInfo::getAsVariant(int resID, QVariant* const value) const
{
    return m_rc.getAsVariant(resID, value);
}

std::unique_ptr<nx::utils::stree::AbstractConstIterator> AuthorizationInfo::begin() const
{
    return m_rc.begin();
}

bool AuthorizationInfo::accessAllowedToOwnDataOnly() const
{
    return false;
}

} // namespace nx::cloud::db
