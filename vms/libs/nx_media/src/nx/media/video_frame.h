// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtMultimedia/QVideoFrame>

namespace nx::media {

/**
 * Wrapper of standard QVideoFrame to support functionality of storing metadata,
 * which was removed in Qt6.
 * */
class VideoFrame: public QVideoFrame
{
public:
    using QVideoFrame::QVideoFrame;

    QMap<QString, QVariant> metaData;
};
} // namespace nx::media
