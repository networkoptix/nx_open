// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "string_replacer.h"

namespace nx::utils::bstream {

StringReplacer::StringReplacer(
    const std::string& before,
    const std::string& after)
    :
    m_replacer(before, after)
{
}

int StringReplacer::write(const void* data, size_t count)
{
    auto replaced = m_replacer.process(std::string(static_cast<const char*>(data), count));
    m_outputStream->write(replaced.data(), replaced.size());
    return (int) count;
}

} // namespace nx::utils::bstream
