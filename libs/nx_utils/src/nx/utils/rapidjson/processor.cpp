// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "processor.h"

#include <rapidjson/writer.h>

namespace nx::utils::rapidjson {

namespace details {

ValueHelper::ValueHelper(const QString& val, ::rapidjson::Document::AllocatorType& alloc):
    allocator(alloc), value(val.toStdString(), allocator)
{
}

ValueHelper& ValueHelper::operator=(ValueHelper&& valueHelper) noexcept
{
    value = std::move(valueHelper.value);
    allocator = std::move(valueHelper.allocator);
    return *this;
}

bool ValueHelper::isNull() const
{
    return value.IsNull();
}

void Path::parse(const std::string& path)
{
    if (path.empty())
        return;

    auto predPos = path.find(conditionToken);
    if (predPos != std::string::npos)
    {
        m_valid = false;
        return;
    }

    if (Token token{path}; token.isValid())
        m_tokens->push_back(std::move(token));
    else
        m_valid = false;
}

int Path::tokenCount() const
{
    return m_tokens->size();
}

bool Path::isValid() const
{
    return m_valid;
}

int Path::conditionCount() const
{
    if (isValid())
    {
        return std::count_if(
            m_tokens->begin(), m_tokens->end(), [](const auto& t) { return t.isCondition(); });
    }
    return 0;
}

Path Path::next() const
{
    Path newPath = *this;
    if (hasNext())
        newPath.m_pos++;
    return newPath;
}

bool Path::hasNext() const
{
    return m_pos < tokenCount() - 1;
}

const Path::Token& Path::currentToken() const
{
    return (*m_tokens)[m_pos];
}

} // namespace details

Processor::Processor(const nx::Buffer& data)
{
    m_data = std::make_shared<::rapidjson::Document>();
    m_data->Parse(data.toByteArray());
    m_curr = &(*m_data);
}

std::string Processor::toStdString() const
{
    ::rapidjson::StringBuffer buffer;
    ::rapidjson::Writer<::rapidjson::StringBuffer> writer(buffer);
    m_curr->Accept(writer);
    return buffer.GetString();
}

bool Processor::isValid() const
{
    return !m_data->HasParseError();
}

::rapidjson::ParseErrorCode Processor::getParseError()
{
    return m_data->GetParseError();
}

void Processor::getWithRecursiveSearch(
    const details::Condition& condition,
    ::rapidjson::Value* parent,
    details::GetValue<details::ReturnCount::All>* action) const
{
    if (parent->IsObject())
        iterateRecursive<details::ObjectPtrHelper>(parent, condition, action);
    else if (parent->IsArray())
        iterateRecursive<details::ArrayPtrHelper>(parent, condition, action);
    else
        return;
}

namespace predicate {

bool NameContains::operator()(const ObjectIterator& root) const
{
    std::string name = root->name.GetString();
    return name.find(m_namePart) != std::string::npos;
}

Compare::Compare(const std::string& name, const QString& val)
{
    m_name = name;
    if (!name.starts_with("/"))
        m_name = "/" + m_name;
    m_value = val;
}

bool Compare::operator()(const ArrayIterator& root) const
{
    ::rapidjson::Pointer pointer(m_name);
    if (!pointer.IsValid())
        return false;
    ::rapidjson::Value* jsonOut = pointer.Get(*root);
    details::TypeHelper<QString> helper;
    auto result = helper.get(jsonOut);
    return result.has_value() && result.value() == m_value;
}

ContainedIn::ContainedIn(
    const std::string& name, const QStringList& values)
{
    m_name = name;
    if (!name.starts_with("/"))
        m_name = "/" + m_name;
    m_values = values;
}

bool ContainedIn::operator()(const ArrayIterator& root) const
{
    ::rapidjson::Pointer pointer(m_name.c_str());
    if (!pointer.IsValid())
        return false;
    ::rapidjson::Value* jsonOut = pointer.Get(*root);
    details::TypeHelper<QString> helper;
    auto result = helper.get(jsonOut);
    return result.has_value() && m_values.contains(result.value());
}

} // namespace predicate

} // namespace nx::utils::rapidjson
