// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_result.h"

#include <QtCore/QCoreApplication>

#include <nx/branding.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::common {

namespace {

class AuthResultTranslations
{
    Q_DECLARE_TR_FUNCTIONS(AuthResultTranslations)

public:
    QString toErrorMessage(AuthResult value) const;
};

QString AuthResultTranslations::toErrorMessage(AuthResult value) const
{
    switch (value)
    {
        case Auth_OK:
            NX_ASSERT(false, "This value is not an error");
            break;

        case Auth_WrongLogin:
        case Auth_WrongInternalLogin:
            return tr("This user does not exist.");

        case Auth_WrongDigest:
        case Auth_WrongPassword:
            return tr("Wrong password.");

        case Auth_PasswordExpired:
            return tr("The password is expired. Please contact your system administrator.");

        case Auth_LDAPConnectError:
            return tr("The LDAP server is not accessible. Please try again later.");

        case Auth_CloudConnectError:
            return tr("%1 is not accessible yet. Please try again later.")
                .arg(nx::branding::cloudName());

        case Auth_DisabledUser:
            return tr("This user has been disabled by a system administrator.");

        case Auth_LockedOut:
            return tr("The user is locked out due to several failed attempts. Please try again later.");

        case Auth_Forbidden:
        case Auth_InvalidCsrfToken:
        case Auth_DisabledBasicAndDigest:
            return tr("This authorization method is forbidden. Please contact your system administrator.");

        case Auth_WrongSessionToken:
            return tr("The session key is invalid or expired.");
    }

    NX_ASSERT(false, "Unhandled value: %1", value);
    return tr("Internal server error (%1). Please contact your system administrator.").arg(value);
}

} // namespace

QString toErrorMessage(AuthResult value)
{
    return AuthResultTranslations().toErrorMessage(value);
}

} // namespace nx::vms::common
