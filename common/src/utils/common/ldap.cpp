#include "ldap.h"

bool QnLdapSettings::equals(const QnLdapSettings &other) const {
    if (host != other.host)                           return false;
    if (adminDn != other.adminDn)                     return false;
    if (adminPassword != other.adminPassword)         return false;
    if (port != other.port)                           return false;
    if (searchBase != other.searchBase)               return false;
    
    return true;
}

bool QnLdapSettings::isValid() const {
    return  !host.isEmpty()
            && !adminDn.isEmpty()
            && !adminPassword.isEmpty()
            && port != -1;
}
