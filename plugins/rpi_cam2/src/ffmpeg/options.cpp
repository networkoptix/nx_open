#include "options.h"

extern "C" {
#include<libavutil/dict.h>
}

#include "error.h"

namespace nx {
namespace ffmpeg {

Options::~Options()
{
    if(m_options)
        av_dict_free(&m_options);
}

int Options::setEntry(const char * key, const char * value, int flags)
{
    int setCode = av_dict_set(&m_options, key, value, flags);
    error::updateIfError(setCode);
    return setCode;
}

AVDictionaryEntry* Options::getEntry(const char * key, const AVDictionaryEntry * prev, int flags) const
{
    return av_dict_get(m_options, key, prev, flags);
}

int Options::count() const
{
    return av_dict_count(m_options);
}

} // namespace ffmpeg
} // namespace nx