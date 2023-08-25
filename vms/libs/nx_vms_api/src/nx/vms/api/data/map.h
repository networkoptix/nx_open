// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

namespace nx::vms::api {

/**
 * Typically used as a return value or an argument of a handler. It implements
 * the `front` and `getId` member functions to handle situations in which CrudHandler expects
 * a vector or when requests are made with an id parameter.
 * Avoids code duplication in key-value CRUD REST API handlers.
 */
template<typename Key, typename Value>
class Map: public std::map<Key, Value>
{
public:
    const Value& front() const { return this->begin()->second; }
    Key getId() const { return (this->size() == 1) ? this->begin()->first : Key(); }
};

} // namespace nx::vms::api
