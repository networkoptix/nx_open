#include "credentials.h"
#include <utils/common/model_functions.h>

QAuthenticator QnCredentials::toAuthenticator() const
{
    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    return auth;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnCredentials, (json)(ubjson)(xml)(csv_record), QnCredentials_Fields)

