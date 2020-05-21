#pragma once

#include <analytics/db/abstract_object_type_dictionary.h>

namespace nx::analytics::db::test {

class ObjectTypeDictionary:
    public AbstractObjectTypeDictionary
{
public:
    virtual std::optional<QString> idToName(const QString& id) const override;
};

} // namespace nx::analytics::db::test
