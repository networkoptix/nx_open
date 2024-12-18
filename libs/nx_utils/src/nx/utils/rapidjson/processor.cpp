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

void Path::parse(std::string_view path)
{
    if (path.empty())
        return;

    auto predPos = path.find(conditionToken);
    if (predPos != std::string_view::npos)
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

Processor::Processor(ConstBufferRefType data)
{
    m_data = std::make_shared<::rapidjson::Document>();
    m_data->Parse(data.data(), data.size());
    m_curr = &(*m_data);
}

std::string Processor::toStdString() const
{
    ::rapidjson::StringBuffer buffer;
    ::rapidjson::Writer<::rapidjson::StringBuffer> writer(buffer);
    m_curr->Accept(writer);
    return std::string{buffer.GetString(), buffer.GetLength()};
}

bool Processor::isValid() const
{
    return !m_data->HasParseError();
}

::rapidjson::ParseErrorCode Processor::getParseError() const
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
}

namespace predicate {

::rapidjson::Pointer pointerFromKey(std::string_view key)
{
    if (key.starts_with('/'))
        return ::rapidjson::Pointer(key.data(), key.size());

    return ::rapidjson::Pointer('/' + std::string{key});
}

bool NameContains::operator()(const ObjectIterator& root) const
{
    const std::string_view name{root->name.GetString(), root->name.GetStringLength()};
    return name.find(m_namePart) != std::string_view::npos;
}

Compare::Compare(std::string_view key, const QString& val):
    m_pointer(pointerFromKey(key)),
    m_value(val)
{
}

bool Compare::operator()(const ArrayIterator& root) const
{
    if (!m_pointer.IsValid())
        return false;
    ::rapidjson::Value* jsonOut = m_pointer.Get(*root);

    using Helper = details::TypeHelper<std::remove_cv_t<decltype(m_value)>>;
    auto result = Helper::get(jsonOut);
    return result.has_value() && result.value() == m_value;
}

ContainedIn::ContainedIn(std::string_view key, const QStringList& values):
    m_pointer(pointerFromKey(key)),
    m_values(values)
{
}

bool ContainedIn::operator()(const ArrayIterator& root) const
{
    if (!m_pointer.IsValid())
        return false;
    ::rapidjson::Value* jsonOut = m_pointer.Get(*root);

    using Helper = details::TypeHelper<std::remove_cv_t<decltype(m_values)::value_type>>;
    auto result = Helper::get(jsonOut);
    return result.has_value() && m_values.contains(result.value());
}

} // namespace predicate

} // namespace nx::utils::rapidjson
