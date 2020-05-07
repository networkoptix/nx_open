#include "object_type_dictionary.h"

#include <nx/utils/cryptographic_hash.h>

namespace nx::analytics::db::test {

std::optional<QString> ObjectTypeDictionary::idToName(const QString& id) const
{
    return nx::utils::QnCryptographicHash::hash(
        id.toUtf8(),
        nx::utils::QnCryptographicHash::Md5).toHex();
}

} // namespace nx::analytics::db::test
