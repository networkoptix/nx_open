#include "password_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/random.h>

#include <utils/common/app_info.h>
#include <nx/utils/crypt/linux_passwd_crypt.h>
#include <utils/crypt/symmetrical.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((PasswordData)(CameraPasswordData), (json), _Fields)

PasswordData::PasswordData()
{
}

PasswordData::PasswordData(const QnRequestParams &params)
{
    password = params.value(lit("password"));
    realm = params.value(lit("realm"));
    passwordHash = params.value(lit("passwordHash")).toLatin1();
    passwordDigest = params.value(lit("passwordDigest")).toLatin1();
    cryptSha512Hash = params.value(lit("cryptSha512Hash")).toLatin1();
}


PasswordData PasswordData::calculateHashes(const QString& username, const QString& password, bool isLdap)
{
    PasswordData result;
    result.realm = nx::network::AppInfo::realm();

    QByteArray salt = QByteArray::number(nx::utils::random::number(), 16);

    QByteArray hash = isLdap ? "LDAP" : "md5";
    hash.append('$');
    hash.append(salt);
    hash.append("$");
    if (isLdap)
    {
        auto encodedPassword = nx::utils::encodeSimple(password.toUtf8(), QByteArray::fromHex(salt));
        hash.append(encodedPassword.toHex());
    }
    else
    {
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(salt);
        md5.addData(password.toUtf8());
        hash.append(md5.result().toHex());
    }
    result.passwordHash = hash;
    result.passwordDigest = nx_http::calcHa1(
        username.toLower(),
        result.realm,
        password);

    result.cryptSha512Hash = linuxCryptSha512(password.toUtf8(), generateSalt(LINUX_CRYPT_SALT_LENGTH));

    return result;
}

bool PasswordData::hasPassword() const
{
    return
        !password.isEmpty() ||
        !passwordHash.isEmpty() ||
        !passwordDigest.isEmpty();
}
