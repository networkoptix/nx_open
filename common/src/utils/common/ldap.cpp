#include "ldap.h"

#include <utils/common/model_functions.h>

namespace {
    const int defaultLdapPort = 389;
}

bool QnLdapSettings::isValid() const {
    return  !uri.isEmpty()
            && !adminDn.isEmpty()
            && !adminPassword.isEmpty();
}

int QnLdapSettings::defaultPort() {
    return defaultLdapPort;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnLdapSettings)(QnLdapUser), (json)(eq), _Fields)
