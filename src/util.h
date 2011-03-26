#ifndef _UNIVERSAL_CLIENT_UTIL_H
#define _UNIVERSAL_CLIENT_UTIL_H

/*
 * Get user data directory. Directory should be available for writing.
 */
QString getDataDirectory();

/*
 * Get media root directory.
 */
QString getMediaRootDir();

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
 * Gets param from string;   for example str= {param1="param_val" sdhksjh}
 * function return param_val
 */
QString getParamFromString(const QString& str, const QString& param);


#endif // _UNIVERSAL_CLIENT_UTIL_H
