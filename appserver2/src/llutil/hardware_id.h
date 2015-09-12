#ifndef _HARDWARE_ID_H_
#define _HARDWARE_ID_H_

#include <stdexcept>
#include <string>

#include <vector>
#include <QtCore/QString>

class QSettings;

namespace LLUtil {

const int LATEST_HWID_VERSION = 4;

QByteArray getHardwareId(int version, bool guidCompatibility, QSettings *settings);
QList<QByteArray> getMainHardwareIds(int guidCompatibility, QSettings *settings);
QList<QByteArray> getCompatibleHardwareIds(int guidCompatibility, QSettings *settings);

}

#endif // _HARDWARE_ID_H_
