#include "ldap.h"

bool QnLdapSettings::equals(const QnLdapSettings &other) const {
    if (uri != other.uri)                           return false;
    if (adminDn != other.adminDn)                     return false;
    if (adminPassword != other.adminPassword)         return false;
    if (searchBase != other.searchBase)               return false;
    
    return true;
}

bool QnLdapSettings::isValid() const {
    return  !uri.isEmpty()
            && !adminDn.isEmpty()
            && !adminPassword.isEmpty();
}
