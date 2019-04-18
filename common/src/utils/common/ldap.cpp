#include "ldap.h"

#include <nx/fusion/model_functions.h>

QString QnLdapSettings::toString() const
{
    return QJson::serialized(*this);
}

bool QnLdapSettings::isValid() const
{
    return !uri.isEmpty()
        && !adminDn.isEmpty()
        && !adminPassword.isEmpty();
}

int QnLdapSettings::defaultPort(bool ssl)
{
    return ssl ? 389 : 636;
}

QString QnLdapUser::toString() const
{
    return QJson::serialized(*this);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnLdapSettings)(QnLdapUser), (json)(eq), _Fields)
