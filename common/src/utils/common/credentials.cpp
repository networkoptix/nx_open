#include "credentials.h"
#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCredentials, (eq)(json), QnCredentials_Fields)

QAuthenticator QnCredentials::toAuthenticator() const
{
    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password.value());

    return auth;
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

bool QnCredentials::operator<(const QnCredentials &other) const
{
    if (user != other.user)
        return user < other.user;

    return password.value() < other.password.value();
}

QString QnCredentials::decodedPassword() const
{
    return password.value();
}
