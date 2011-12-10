#ifndef _UNIVERSAL_CLIENT_UTIL_H
#define _UNIVERSAL_CLIENT_UTIL_H

#include <QString>

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

/*
 * Remove directory recursively.
 */
QN_EXPORT bool removeDir(const QString &dirName);

/*
 * Convert path from native to inner.
 */
QN_EXPORT QString fromNativePath(QString path);

/*
 * Get user data directory. Directory should be available for writing.
 */
QN_EXPORT QString getDataDirectory();

/*
 * Get user movies directory.
 */
QN_EXPORT QString getMoviesDirectory();

/*
 * Get number of digits in decimal representaion of an integer.
 */
QN_EXPORT int digitsInNumber(unsigned num);

/*
 * Format duration like HH:MM:SS/HH:MM:SS
 *
 * @param duration duration in seconds
 * @param total duration in seconds
 */
QN_EXPORT QString formatDuration(unsigned duration, unsigned total = 0);

/*
 * Gets param from string;   for example str= {param1="param_val" sdhksjh}
 * function return param_val
 */
QN_EXPORT QString getParamFromString(const QString& str, const QString& param);

/*
 * Round value up with step. step must be power of 2
 * function return rounded value
 */
inline unsigned int roundUp(unsigned int value, int step) {
    return ((value-1) & ~(step-1)) + step;
}

QN_EXPORT QString strPadLeft(const QString &str, int len, char ch);

QN_EXPORT QString closeDirPath(const QString& value);

QN_EXPORT qint64 getDiskFreeSpace(const QString& root);

#ifndef INT64_MAX
static const qint64 INT64_MAX = 0x7fffffffffffffffll;
static const qint64 INT64_MIN = 0x8000000000000000ll;
#endif

static const qint64 DATETIME_NOW = INT64_MAX;

static const char *DEFAULT_APPSERVER_HOST = "127.0.0.1";
static const int DEFAULT_APPSERVER_PORT = 8000;

static const int MAX_RTSP_DATA_LEN = 65535 - 16;

//static const qint64 BACKWARD_SEEK_STEP =  2000 * 1000; 
static const qint64 BACKWARD_SEEK_STEP =  1000 * 1000; 
static const qint64 MAX_FIRST_GOP_DURATION = 1000 * 1000 * 10;

static const qint64 MAX_FRAME_DURATION = 10 * 1000;

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
#define htonll(x) \
    ((((x) & 0xff00000000000000LL) >> 56) | \
    (((x) & 0x00ff000000000000LL) >> 40) | \
    (((x) & 0x0000ff0000000000LL) >> 24) | \
    (((x) & 0x000000ff00000000LL) >> 8) | \
    (((x) & 0x00000000ff000000LL) << 8) | \
    (((x) & 0x0000000000ff0000LL) << 24) | \
    (((x) & 0x000000000000ff00LL) << 40) | \
    (((x) & 0x00000000000000ffLL) << 56))
#else
#define htonll(x) 
#endif

#endif // _UNIVERSAL_CLIENT_UTIL_H
