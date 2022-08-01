// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace nx::reflect::json {

std::tuple<std::string, DeserializationResult> compactJson(const std::string_view json)
{
    using namespace rapidjson;

    Document document;
    document.Parse(json.data(), json.size());
    if (document.HasParseError())
    {
        return std::make_tuple(
            std::string(),
            DeserializationResult(false, json_detail::parseErrorToString(document), std::string(json)));
    }

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    document.Accept(writer);
    return std::make_tuple(
        std::string(buffer.GetString(), buffer.GetSize()),
        DeserializationResult(true));
}

} // namespace nx::reflect::json

//--------------------------------------------------------------------------------------------------

namespace nx::reflect::json_detail {

static const char* toString(const rapidjson::ParseErrorCode& val)
{
    switch (val)
    {
        case rapidjson::kParseErrorDocumentEmpty:
            return "DocumentEmpty";
        case rapidjson::kParseErrorDocumentRootNotSingular:
            return "DocumentRootNotSingular";
        case rapidjson::kParseErrorValueInvalid:
            return "ValueInvalid";
        case rapidjson::kParseErrorObjectMissName:
            return "ObjectMissName";
        case rapidjson::kParseErrorObjectMissColon:
            return "ObjectMissColon";
        case rapidjson::kParseErrorObjectMissCommaOrCurlyBracket:
            return "ObjectMissCommaOrCurlyBracket";
        case rapidjson::kParseErrorArrayMissCommaOrSquareBracket:
            return "ArrayMissCommaOrSquareBracket";
        case rapidjson::kParseErrorStringUnicodeEscapeInvalidHex:
            return "StringUnicodeEscapeInvalidHex";
        case rapidjson::kParseErrorStringUnicodeSurrogateInvalid:
            return "StringUnicodeSurrogateInvalid";
        case rapidjson::kParseErrorStringEscapeInvalid:
            return "StringEscapeInvalid";
        case rapidjson::kParseErrorStringMissQuotationMark:
            return "StringMissQuotationMark";
        case rapidjson::kParseErrorStringInvalidEncoding:
            return "StringInvalidEncoding";
        case rapidjson::kParseErrorNumberTooBig:
            return "NumberTooBig";
        case rapidjson::kParseErrorNumberMissFraction:
            return "NumberMissFraction";
        case rapidjson::kParseErrorNumberMissExponent:
            return "NumberMissExponent";
        case rapidjson::kParseErrorTermination:
            return "Termination";
        case rapidjson::kParseErrorUnspecificSyntaxError:
            return "UnspecificSyntaxError";
        default:
            return "Unknown";
    }
}

using namespace rapidjson;

std::string getStringRepresentation(const Value& val)
{
    StringBuffer strbuf;
    strbuf.Clear();
    Writer<StringBuffer> writer(strbuf);
    val.Accept(writer);
    return strbuf.GetString();
}

std::string parseErrorToString(const rapidjson::ParseResult& parseResult)
{
    std::string str;
    str += "Parse error \"";
    str += toString(parseResult.Code());
    str += "\" at position ";
    str += std::to_string(parseResult.Offset());
    return str;
}

} // namespace nx::reflect::json_detail
