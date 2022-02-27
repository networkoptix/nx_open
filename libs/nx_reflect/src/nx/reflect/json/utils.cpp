// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace nx::reflect::json_detail {

using namespace rapidjson;

std::string getStringRepresentation(const Value& val)
{
    StringBuffer strbuf;
    strbuf.Clear();
    Writer<StringBuffer> writer(strbuf);
    val.Accept(writer);
    return strbuf.GetString();
}

} // namespace nx::reflect::json_detail