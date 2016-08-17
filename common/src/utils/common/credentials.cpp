#include "credentials.h"
#include <utils/common/model_functions.h>

QAuthenticator QnCredentials::toAuthenticator() const
{
    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    return auth;
}

bool QnCredentials::operator==(const QnCredentials& other) const
{
    return user == other.user && password == other.password;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCredentials, (json)(ubjson)(xml)(csv_record), QnCredentials_Fields)

