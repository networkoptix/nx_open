// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QDateTime>
#include <QtCore/QString>

NX_VMS_COMMON_API QString strPadLeft(const QString &str, int len, char ch);

NX_VMS_COMMON_API QString closeDirPath(const QString &value);
NX_VMS_COMMON_API QString getPathSeparator(const QString& path);

//#define MAX_RTSP_DATA_LEN (65535 - 16)

#define BACKWARD_SEEK_STEP (1000ll * 1000)

// This constant limit duration of a first GOP. So, if jump to position X, and first I-frame has position X-N, data will be transfer for range
// [X-N..X-N+MAX_FIRST_GOP_DURATION]. It prevent long preparing then reverse mode is activate.
// I have increased constant because of camera with very low FPS may have long gop (30-60 seconds).
//static const qint64 MAX_FIRST_GOP_DURATION = 1000 * 1000 * 10;
#define MAX_FIRST_GOP_FRAMES 250ll

#define MAX_FRAME_DURATION_MS (5ll * 1000)
#define MAX_FRAME_DURATION std::chrono::milliseconds(MAX_FRAME_DURATION_MS)

#define MIN_FRAME_DURATION_USEC 16667ll
#define MIN_FRAME_DURATION std::chrono::microseconds(MIN_FRAME_DURATION_USEC)

#define MAX_AUDIO_FRAME_DURATION_MS 150ll
#define MAX_AUDIO_FRAME_DURATION std::chrono::milliseconds(MAX_AUDIO_FRAME_DURATION_MS)

NX_VMS_COMMON_API QString getValueFromString(const QString& line);

static const qint64 UTC_TIME_DETECTION_THRESHOLD = 1'000'000ll * 3600*24*100;

/**
 * \returns                             has of string. Added for compatibility with QT4 code
 */
NX_VMS_COMMON_API uint qt4Hash(const QString& key);

NX_VMS_COMMON_API QString mksecToDateTime(qint64 valueUsec);
