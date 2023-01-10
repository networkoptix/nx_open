// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <memory>

#include <QString>

#include <nx/streaming/av_codec_media_context.h>

class NX_VMS_COMMON_API AudioLayout
{
public:
    struct AudioTrack
    {
        QString description;
        CodecParametersConstPtr codecParams;
    };

public:
    AudioLayout() = default;
    AudioLayout(const CodecParametersConstPtr& codecParams);
    AudioLayout(const AVFormatContext* formatContext);

    void addTrack(const CodecParametersConstPtr &codecParams, const QString& description = "");
    const std::vector<AudioTrack>& tracks() const;

private:
    AudioLayout(const AudioLayout&) = delete;
    AudioLayout& operator=(const AudioLayout&) = delete;

private:
    std::vector<AudioTrack> m_audioTracks;
};
