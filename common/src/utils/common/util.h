#ifndef _UNIVERSAL_CLIENT_UTIL_H
#define _UNIVERSAL_CLIENT_UTIL_H

#include <QString>
#include "math.h" /* For INT64_MAX. */

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

/**
 * Remove directory recursively.
 * 
 * \param dirName                       Name of the directory to remove.
 * \returns                             Whether the operation completer successfully.
 */
QN_EXPORT bool removeDir(const QString &dirName);

/**
 * Convert path from native to universal.
 * 
 * \param path                          Path to convert.
 * \returns                             Converted path.
 */
QN_EXPORT QString fromNativePath(QString path);

/**
 * \returns                             User movies directory.
 */
QN_EXPORT QString getMoviesDirectory();

/**
 * \param num                           Number.
 * \returns                             Number of digits in decimal representation of the given number.
 */
QN_EXPORT int digitsInNumber(unsigned num);

/**
 * Formats position in a time interval using the "HH:MM:SS/HH:MM:SS" format.
 * Hours part is omitted when it is not relevant.
 *
 * \param position                      Current position in seconds.
 * \param total                         Total duration in seconds. 
 *                                      Pass zero to convert position only.
 */
QN_EXPORT QString formatDuration(unsigned position, unsigned total = 0);

/**
 * Gets param from string;   for example str= {param1="param_val" sdhksjh}
 * function return param_val
 */
QN_EXPORT QString getParamFromString(const QString& str, const QString& param);

QN_EXPORT QString strPadLeft(const QString &str, int len, char ch);

QN_EXPORT QString closeDirPath(const QString& value);

QN_EXPORT qint64 getDiskFreeSpace(const QString& root);

static const qint64 DATETIME_NOW = INT64_MAX;
#define DATETIME_NOW DATETIME_NOW /* Get some syntax highlighting. */

static const char *DEFAULT_APPSERVER_HOST = "127.0.0.1";
static const int DEFAULT_APPSERVER_PORT = 8000;

static const int MAX_RTSP_DATA_LEN = 65535 - 16;

//static const qint64 BACKWARD_SEEK_STEP =  2000 * 1000; 
static const qint64 BACKWARD_SEEK_STEP =  1000 * 1000; 

// This constant limit duration of a first GOP. So, if jump to position X, and first I-frame has position X-N, data will be transfer for range
// [X-N..X-N+MAX_FIRST_GOP_DURATION]. It prevent long preparing then reverse mode is activate.
// I have increased constant because of camera with very low FPS may have long gop (30-60 seconds).
//static const qint64 MAX_FIRST_GOP_DURATION = 1000 * 1000 * 10;
static const qint64 MAX_FIRST_GOP_FRAMES = 250;

static const qint64 MAX_FRAME_DURATION = 5 * 1000;
static const qint64 MIN_FRAME_DURATION = 15;

quint64 QN_EXPORT getUsecTimer();


#endif // _UNIVERSAL_CLIENT_UTIL_H
