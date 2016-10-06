#pragma once

#include <array>
#include <cstdint>

#include <QtCore/QVariant>

namespace nx {
namespace db {

struct SqlFilterField
{
    const char* fieldName;
    const char* placeHolderName;
    QVariant fieldValue;
};

typedef std::vector<SqlFilterField> InnerJoinFilterFields;

} // namespace db
} // namespace nx
