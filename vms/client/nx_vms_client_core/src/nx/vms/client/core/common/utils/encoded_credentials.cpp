#include "encoded_credentials.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::core {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EncodedCredentials, (eq)(json), EncodedCredentials_Fields)

EncodedCredentials::EncodedCredentials(
    const QString& user,
    const QString& password,
    const QByteArray& key)
    :
    user(user),
    password(EncodedString::fromDecoded(password, key))
{
}

EncodedCredentials::EncodedCredentials(const nx::utils::Url& url, const QByteArray& key):
    EncodedCredentials(url.userName(), url.password(), key)
{
}

QByteArray EncodedCredentials::key() const
{
    return password.key();
}

void EncodedCredentials::setKey(const QByteArray& key)
{
    password.setKey(key);
}

QString EncodedCredentials::decodedPassword() const
{
    return password.decoded();
}

bool EncodedCredentials::isEmpty() const
{
    return user.isEmpty() && password.isEmpty();
}

bool EncodedCredentials::isValid() const
{
    return !user.isEmpty() && !password.isEmpty();
}

void PrintTo(const EncodedCredentials& value, ::std::ostream* os)
{
    *os << value.user.toStdString() << ": ";
    PrintTo(value.password, os);
}

} // namespace nx::vms::client::core
