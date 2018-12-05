#include "encoded_credentials.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnEncodedCredentials, (eq)(json), QnEncodedCredentials_Fields)

QnEncodedCredentials::QnEncodedCredentials(
    const QString& user,
    const QString& password,
    const QByteArray& key)
    :
    user(user),
    password(password, key)
{
}

QnEncodedCredentials::QnEncodedCredentials(const nx::utils::Url& url, const QByteArray& key):
    user(url.userName()),
    password(url.password(), key)
{
}

QString QnEncodedCredentials::decodedPassword() const
{
    return password.value();
}

bool QnEncodedCredentials::isEmpty() const
{
    return user.isEmpty() && password.isEmpty();
}

bool QnEncodedCredentials::isValid() const
{
    return !user.isEmpty() && !password.isEmpty();
}
