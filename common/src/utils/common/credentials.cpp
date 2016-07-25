#include "credentials.h"
#include <nx/fusion/model_functions.h>

QAuthenticator QnCredentials::toAuthenticator() const
{
    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    return auth;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnCredentials), (json)(ubjson)(xml)(csv_record), _Fields)
