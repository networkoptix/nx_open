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
    const int defaultLdapPort = 389;
    const int defaultLdapSslPort = 636;
    return ssl ? defaultLdapSslPort : defaultLdapPort;
}

QString QnLdapUser::toString() const
{
    return QJson::serialized(*this);
}


QString toString(const QnLdapSettings& settings)
{
    if (nx::utils::log::showPasswords())
        return settings.toString();

    auto copy = settings;
    copy.adminPassword = "******";
    return copy.toString();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnLdapSettings)(QnLdapUser), (json)(eq), _Fields)
