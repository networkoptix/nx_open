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

#endif // _UNIVERSAL_CLIENT_UTIL_H
