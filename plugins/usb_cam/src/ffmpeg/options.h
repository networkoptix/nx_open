#pragma once

struct AVDictionary;
struct AVDictionaryEntry;

namespace nx {
namespace ffmpeg {

class Options
{
public:
    ~Options();
    int setEntry(const char * key, const char * value, int flags = 0);
    AVDictionaryEntry* getEntry(const char * key, const AVDictionaryEntry * prev = nullptr, int flags = 0) const;
    int count() const;

protected:
    AVDictionary * m_options;
};


} // namespace ffmpeg
} // namespace nx