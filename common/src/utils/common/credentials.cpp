#include "credentials.h"
#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCredentials, (eq)(json)(ubjson)(xml)(csv_record), QnCredentials_Fields)

QAuthenticator QnCredentials::toAuthenticator() const
{
    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    return auth;
}

bool QnCredentials::isNull() const
{
    return user.isNull() && password.isNull();
}

bool QnCredentials::isEmpty() const
{
    return user.isEmpty() && password.isEmpty();
}

bool QnCredentials::isValid(bool allowEmptyPassword) const
{
    if (user.isEmpty())
        return false;

    return allowEmptyPassword || !password.isEmpty();
}
