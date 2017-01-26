#include "ldap.h"

#include <nx/fusion/model_functions.h>

namespace {
    const int defaultLdapPort = 389;
    const int defaultLdapSslPort = 636;
}

bool QnLdapSettings::isValid() const {
    return  !uri.isEmpty()
            && !adminDn.isEmpty()
            && !adminPassword.isEmpty();
}

int QnLdapSettings::defaultPort(bool ssl) {
    return ssl 
        ? defaultLdapSslPort 
        : defaultLdapPort;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnLdapSettings)(QnLdapUser), (json)(eq), _Fields)
