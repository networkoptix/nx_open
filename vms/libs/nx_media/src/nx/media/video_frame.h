// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QString>
#include <QVariant>

#include <QtMultimedia/QVideoFrame>

#include <nx/utils/impl_ptr.h>

namespace nx::media {

/**
 * Wrapper of standard QVideoFrame to support functionality of storing metadata,
 * which was removed in Qt6.
 * */
class VideoFrame: public QVideoFrame
{
    using base_type = QVideoFrame;

public:
    explicit VideoFrame();
    VideoFrame(const QVideoFrameFormat &format);
    VideoFrame(QAbstractVideoBuffer *buffer, const QVideoFrameFormat &format);
    virtual ~VideoFrame();

    void setMetadata(const QString& key, const QVariant& value);
    QVariant metaData(const QString& key) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
} // namespace nx::media
