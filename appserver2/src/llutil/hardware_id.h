#ifndef _HARDWARE_ID_H_
#define _HARDWARE_ID_H_

#include <stdexcept>
#include <string>

#include <vector>
#include <QtCore/QString>

#include "licensing/hardware_info.h"

class QSettings;

namespace LLUtil {

const int LATEST_HWID_VERSION = 4;

const QnHardwareInfo &getHardwareInfo();
QString getHardwareId(int version, bool guidCompatibility, QSettings *settings);
QStringList getMainHardwareIds(int guidCompatibility, QSettings *settings);
QStringList getCompatibleHardwareIds(int guidCompatibility, QSettings *settings);

}

#endif // _HARDWARE_ID_H_
