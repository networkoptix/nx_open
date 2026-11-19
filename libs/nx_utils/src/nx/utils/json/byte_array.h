// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

#include <rapidjson/document.h>

namespace nx::json {

/**
 * RapidJSON output stream that writes the text directly into QByteArray. Can be used to avoid
 * std::string conversion.
 */
class NX_UTILS_API ByteArrayOutputStream
{
public:
    using Ch = QByteArray::value_type;

    // Keep the method inline to avoid the overhead for cross-DLL call per byte.
    void Put(Ch c)
    {
        static constexpr qsizetype kGrowStep = 4096;

        if (m_size == m_buffer.size())
        {
            m_buffer.resizeForOverwrite(m_size + kGrowStep);
            m_data = m_buffer.data(); //< data() checks for sharing, so do not call it per byte.
        }
        m_data[m_size++] = c;
    }

    void Flush() {}
    QByteArray take();

private:
    QByteArray m_buffer;
    Ch* m_data = nullptr;
    qsizetype m_size = 0;
};

/** Writes the value as compact JSON text. */
NX_UTILS_API QByteArray toByteArray(const rapidjson::Value& value);

} // namespace nx::json
