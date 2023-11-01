// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_frame.h"

#include <unordered_map>

namespace nx::media {

struct VideoFrame::Private
{
    std::unordered_map<QString, QVariant> metaData;
};

VideoFrame::VideoFrame():
    base_type(),
    d(new Private())
{
}

VideoFrame::VideoFrame(const QVideoFrameFormat& format):
    base_type(format),
    d(new Private())
{
}

VideoFrame::~VideoFrame()
{
}

VideoFrame::VideoFrame(QAbstractVideoBuffer *buffer, const QVideoFrameFormat &format):
    base_type(buffer, format),
    d(new Private())
{
}

void VideoFrame::setMetadata(const QString& key, const QVariant& value)
{
    d->metaData[key] = value;
}

QVariant VideoFrame::metaData(const QString& key) const
{
    const auto it = d->metaData.find(key);
    return it != d->metaData.end() ? it->second : QVariant();
}

} // namespace nx::media
