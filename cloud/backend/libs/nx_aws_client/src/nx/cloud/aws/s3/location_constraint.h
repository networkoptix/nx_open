#pragma once

#include <QString>

namespace nx::cloud::aws {

namespace s3 {

struct NX_AWS_CLIENT_API LocationConstraint
{
    std::string region;
};

} // namespace s3

namespace xml {

//NOTE: No template specializations for this type because it has variable xml...
bool deserialize(const QByteArray& data, s3::LocationConstraint* outObject);

QByteArray serialized(const s3::LocationConstraint& object);

} // namespace xml
} // namespace nx::cloud::aws
