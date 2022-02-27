// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "password_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/random.h>

#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <utils/crypt/symmetrical.h>

#include <core/resource/user_resource.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PasswordData, (json), PasswordData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraPasswordData, (json), CameraPasswordData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CurrentPasswordData, (json), CurrentPasswordData_Fields)

PasswordHashes PasswordHashes::calculateHashes(
    const QString& username, const QString& password, bool isLdap)
{
    PasswordData result;
    result.realm = nx::network::AppInfo::realm().c_str();

    result.passwordHash = isLdap
        ? QnUserHash::ldapPassword(password).toString()
        : QnUserHash::scryptPassword(password, nx::scrypt::Options{}).toString();

    result.passwordDigest = nx::network::http::calcHa1(
        username.toLower().toStdString(),
        result.realm.toStdString(),
        password.toStdString()).c_str();

    result.cryptSha512Hash = nx::utils::linuxCryptSha512(
        password.toUtf8(),
        nx::utils::generateSalt(nx::utils::kLinuxCryptSaltLength));

    return result;
}

bool PasswordData::hasPassword() const
{
    return
        !password.isEmpty() ||
        !passwordHash.isEmpty() ||
        !passwordDigest.isEmpty();
}
