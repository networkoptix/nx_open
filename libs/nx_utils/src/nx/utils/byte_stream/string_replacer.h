#pragma once

#include <nx/utils/algorithm/kmp.h>

#include "pipeline.h"

namespace nx::utils::bstream {

/**
 * Replaces given pattern with a given string.
 * NOTE: Some data may be cached inside. To flush the cache, invoke StringReplacer::write with 
 * no data.
 */
class NX_UTILS_API StringReplacer:
    public AbstractOutputConverter
{
public:
    StringReplacer(
        const std::string& before,
        const std::string& after);

    virtual int write(const void* data, size_t count) override;

private:
    algorithm::KmpReplacer m_replacer;
};

} // namespace nx::utils::bstream
