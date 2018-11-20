#pragma once

#include <chrono>

#include <QtCore/QString>

/**
 * Remove directory recursively.
 *
 * \param dirName                       Name of the directory to remove.
 * \returns                             Whether the operation completer successfully.
 */
bool removeDir(const QString &dirName);

/**
 * Convert path from native to universal.
 *
 * \param path                          Path to convert.
 * \returns                             Converted path.
 */
QString fromNativePath(const QString &path);

/**
 * \returns                             User movies directory.
 */
QString getMoviesDirectory();

/**
 * \returns                             User backrounds directory.
 */
QString getBackgroundsDirectory();

/**
 * \param num                           Number.
 * \returns                             Number of digits in decimal representation of the given number.
 */
int digitsInNumber(unsigned num);

/**
 * Gets param from string;   for example str= {param1="param_val" sdhksjh}
 * function return param_val
 */
QString getParamFromString(const QString &str, const QString &param);

QString strPadLeft(const QString &str, int len, char ch);

QString closeDirPath(const QString &value);
QString getPathSeparator(const QString& path);

qint64 getDiskFreeSpace(const QString &root);

qint64 getDiskTotalSpace(const QString &root);

// TODO: #ynikitenkov move to cpp, get rid of define
#define DEFAULT_APPSERVER_HOST "127.0.0.1"
static constexpr int kDefaultConnectionPort = 7001;

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

quint64 getUsecTimer();

// TODO: #Elric move to time.h
// TODO: Rename to reflect that result is in seconds.
/*
 * \returns                             Current time zone offset in seconds.
 */
int currentTimeZone();

static const qint64 UTC_TIME_DETECTION_THRESHOLD = 1000000ll * 3600*24*100;

/**
 * \returns                             has of string. Added for compatibility with QT4 code
 */
uint qt4Hash(const QString& key);

#ifdef _DEBUG
QString debugTime(qint64 timeMSec, const QString &fmt = QString());
#endif


QString mksecToDateTime(qint64 valueUsec);
