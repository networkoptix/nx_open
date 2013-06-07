#ifndef _UNIVERSAL_CLIENT_UTIL_H
#define _UNIVERSAL_CLIENT_UTIL_H

#include <common/config.h>

#include <QtCore/QString>

#include <utils/math/math.h> /* For INT64_MAX. */

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
QN_EXPORT QString formatDuration(unsigned position, unsigned total = 0); // TODO: #Elric move to string.h

/**
 * Gets param from string;   for example str= {param1="param_val" sdhksjh}
 * function return param_val
 */
QN_EXPORT QString getParamFromString(const QString& str, const QString& param);

QN_EXPORT QString strPadLeft(const QString &str, int len, char ch);

QN_EXPORT QString closeDirPath(const QString& value);

QN_EXPORT qint64 getDiskFreeSpace(const QString& root);

QN_EXPORT qint64 getDiskTotalSpace(const QString& root);

#define DATETIME_NOW INT64_MAX 

#define DEFAULT_APPSERVER_HOST "127.0.0.1"
#define DEFAULT_APPSERVER_PORT 7001

//#define MAX_RTSP_DATA_LEN (65535 - 16)
#define MAX_RTSP_DATA_LEN (16*1024 - 16)

#define BACKWARD_SEEK_STEP (1000ll * 1000)

// This constant limit duration of a first GOP. So, if jump to position X, and first I-frame has position X-N, data will be transfer for range
// [X-N..X-N+MAX_FIRST_GOP_DURATION]. It prevent long preparing then reverse mode is activate.
// I have increased constant because of camera with very low FPS may have long gop (30-60 seconds).
//static const qint64 MAX_FIRST_GOP_DURATION = 1000 * 1000 * 10;
#define MAX_FIRST_GOP_FRAMES 250ll

#define MAX_FRAME_DURATION (5ll * 1000)
#define MIN_FRAME_DURATION 16667ll
#define MAX_AUDIO_FRAME_DURATION 150ll

quint64 QN_EXPORT getUsecTimer();

/*
* Returns current time zone offset in seconds
*/
int currentTimeZone(); // TODO: #Elric move to time.h


static const qint64 UTC_TIME_DETECTION_THRESHOLD = 1000000ll * 3600*24*100;


/**
 * \param min
 * \param max
 * \returns                             Random number in range [min, max).
 */
int random(int min, int max);



#endif // _UNIVERSAL_CLIENT_UTIL_H
