#ifndef _UNIVERSAL_CLIENT_UTIL_H
#define _UNIVERSAL_CLIENT_UTIL_H

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

/*
 * Remove directory recursively.
 */
bool removeDir(const QString &dirName);

/*
 * Convert path from native to inner.
 */
QString fromNativePath(QString path);

/*
 * Get user data directory. Directory should be available for writing.
 */
QString getDataDirectory();

/*
 * Get user movies directory.
 */
QString getMoviesDirectory();

/*
 * Get temp directory for video recording.
 */
QString getTempRecordingDir();

/*
 * Get directory to save recorded video.
 */
QString getRecordingDir();

/*
 * Get number of digits in decimal representaion of an integer.
 */
int digitsInNumber(unsigned num);

/*
 * Format duration like HH:MM:SS/HH:MM:SS
 *
 * @param duration duration in seconds
 * @param total duration in seconds
 */
QString formatDuration(unsigned duration, unsigned total = 0);

/*
 * Gets param from string;   for example str= {param1="param_val" sdhksjh}
 * function return param_val
 */
QString getParamFromString(const QString& str, const QString& param);

/*
 * Round value up with step. step must be power of 2
 * function return rounded value
 */
inline int roundUp(int value, int step) {
    return ((value-1) & ~(step-1)) + step;
}

QString strPadLeft(const QString &str, int len, char ch);

#endif // _UNIVERSAL_CLIENT_UTIL_H
