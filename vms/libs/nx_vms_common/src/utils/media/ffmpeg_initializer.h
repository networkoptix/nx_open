// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

/** RAII class for correct ffmpeg initializing and deinitializing. */
class NX_VMS_COMMON_API QnFfmpegInitializer: public QObject
{
    Q_OBJECT
public:
    QnFfmpegInitializer(QObject* parent = nullptr);
    ~QnFfmpegInitializer();
};
