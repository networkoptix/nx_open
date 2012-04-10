#ifndef _UNIVERSAL_CLIENT_UTIL_H
#define _UNIVERSAL_CLIENT_UTIL_H

#include <QString>

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
 * \returns                             User data directory. Directory should be available for writing.
 */
QN_EXPORT QString getDataDirectory();

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

/**
 * \param value                         Value to round up.
 * \param step                          Rounding step, must be power of 2.
 * \returns                             Rounded value.
 */
inline unsigned int roundUp(unsigned int value, int step) {
    return ((value-1) & ~(step-1)) + step;
}

inline quint64 roundUp(quint64 value, int step) {
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

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
inline static quint64 htonll(quint64 x)
{
    return
    ((((x) & 0xff00000000000000ULL) >> 56) | 
    (((x) & 0x00ff000000000000ULL) >> 40) | 
    (((x) & 0x0000ff0000000000ULL) >> 24) | 
    (((x) & 0x000000ff00000000ULL) >> 8) | 
    (((x) & 0x00000000ff000000ULL) << 8) | 
    (((x) & 0x0000000000ff0000ULL) << 24) | 
    (((x) & 0x000000000000ff00ULL) << 40) | 
    (((x) & 0x00000000000000ffULL) << 56));
}
inline static quint64 ntohll(quint64 x) { return htonll(x); }

#else
inline static quint64 htonll(quint64 x) { return x;}
inline static quint64 ntohll(quint64 x)  { return x;}
#endif

/**
 * \param value                         Value to check.
 * \param min                           Interval's left border.
 * \param max                           Interval's right border.
 * \returns                             Whether the given value lies in [min, max) interval.
 */
template<class T>
bool qBetween(const T &value, const T &min, const T &max) {
    return min <= value && value < max;
}

quint64 QN_EXPORT getUsecTimer();


#endif // _UNIVERSAL_CLIENT_UTIL_H
