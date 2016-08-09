#include "password_data.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((PasswordData), (json), _Fields)

PasswordData::PasswordData()
{

}

PasswordData::PasswordData(const QnRequestParams &params)
{
    password = params.value(lit("password"));
    realm = params.value(lit("realm")).toLatin1();
    passwordHash = params.value(lit("passwordHash")).toLatin1();
    passwordDigest = params.value(lit("passwordDigest")).toLatin1();
    cryptSha512Hash = params.value(lit("cryptSha512Hash")).toLatin1();
}

bool PasswordData::hasPassword() const
{
    return
        !password.isEmpty() ||
        !passwordHash.isEmpty() ||
        !passwordDigest.isEmpty();
}
