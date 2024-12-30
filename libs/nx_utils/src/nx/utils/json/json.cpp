// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json.h"

namespace nx::json {

namespace details {

void StripDefaultComposer::startArray()
{
    m_structured.push_back({});
}

void StripDefaultComposer::endArray(int)
{
    if (m_structured.back().writtenCount != 0)
        Composer::endArray(m_structured.back().writtenCount);
    m_structured.pop_back();
}

void StripDefaultComposer::startObject()
{
    m_structured.push_back({});
}

void StripDefaultComposer::endObject(int)
{
    if (m_structured.back().writtenCount != 0)
        Composer::endObject(m_structured.back().writtenCount);
    m_structured.pop_back();
}

void StripDefaultComposer::writeBool(bool value)
{
    onWriteValue();
    Composer::writeBool(value);
}

void StripDefaultComposer::writeInt(const std::int64_t& value)
{
    onWriteValue();
    Composer::writeInt(value);
}

void StripDefaultComposer::writeFloat(const double& value)
{
    onWriteValue();
    Composer::writeFloat(value);
}

void StripDefaultComposer::writeString(const std::string_view& value)
{
    onWriteValue();
    Composer::writeString(value);
}

void StripDefaultComposer::writeNull()
{
    onWriteValue();
    Composer::writeNull();
}

void StripDefaultComposer::writeAttributeName(const std::string_view& name)
{
    if (m_structured.back().writtenCount != 0)
        Composer::writeAttributeName(name);
    else
        m_structured.back().firstAttributeWritten = name;
}

void StripDefaultComposer::onWriteValue()
{
    if (!m_structured.empty())
        addValue(m_structured.rbegin());
}

void StripDefaultComposer::addValue(std::vector<Structured>::reverse_iterator structured)
{
    if (structured->writtenCount != 0)
    {
        ++structured->writtenCount;
        return;
    }

    if (auto next = std::next(structured); next != m_structured.rend())
        addValue(next);
    structured->start(this);
    ++structured->writtenCount;
}

} // namespace details

} // namespace nx::json
