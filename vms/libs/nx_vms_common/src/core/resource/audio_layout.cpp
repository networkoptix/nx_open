// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_layout.h"

AudioLayout::AudioLayout(const CodecParametersConstPtr& codecParams)
{
    addTrack(codecParams);
}

AudioLayout::AudioLayout(const AVFormatContext* formatContext)
{
    for (uint32_t i = 0; i < formatContext->nb_streams; ++i)
    {
        AVStream* stream = formatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            QString description;
            AVDictionaryEntry* lang = av_dict_get(stream->metadata, "language", 0, 0);

            if (lang && lang->value && lang->value[0])
                description = QString(lang->value) + " - ";

            auto codecParams = std::make_shared<CodecParameters>(stream->codecpar);
            description += codecParams->getAudioCodecDescription();
            addTrack(codecParams, description);
        }
    }
}

void AudioLayout::addTrack(const CodecParametersConstPtr& codecParams, const QString& description)
{
    m_audioTracks.push_back({description, codecParams});
}

const std::vector<AudioLayout::AudioTrack>& AudioLayout::tracks() const
{
     return m_audioTracks;
}
