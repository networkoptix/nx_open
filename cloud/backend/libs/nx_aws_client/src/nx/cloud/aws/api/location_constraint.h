#pragma once

#include <QByteArray>

namespace nx::cloud::aws::api {

struct NX_AWS_CLIENT_API LocationConstraint
{
    std::string region;
};

namespace xml {

bool deserialize(const QByteArray& data, LocationConstraint* outObject);

} // namespace xml

} // namespace nx::cloud::aws::api