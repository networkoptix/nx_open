// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object.h"

namespace nx::reflect::json::detail {

bool Object::contains(const std::string& attrName) const
{
    return m_attrs.contains(attrName);
}

DeserializationResult deserialize(const DeserializationContext& ctx, Object* data)
{
    return json_detail::deserialize(ctx, &data->m_attrs);
}

void serialize(SerializationContext* ctx, const Object& data)
{
    nx::reflect::BasicSerializer::serialize(ctx, data.m_attrs);
}

} // namespace nx::reflect::json::detail
