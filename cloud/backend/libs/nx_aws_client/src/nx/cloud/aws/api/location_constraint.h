#pragma once

#include <QString>

namespace nx::cloud::aws::api {

struct NX_AWS_CLIENT_API LocationConstraint
{
    std::string region;
};

namespace xml {

//NOTE: No template specializations for this type because it has variable serialized types
bool deserialize(const QByteArray& data, LocationConstraint* outObject);

QByteArray serialized(const LocationConstraint& object);

} // namespace xml

} // namespace nx::cloud::aws::api
