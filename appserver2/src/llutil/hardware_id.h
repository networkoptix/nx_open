#ifndef _HARDWARE_ID_H_
#define _HARDWARE_ID_H_

#include <stdexcept>
#include <string>

#include <vector>
#include <QtCore/QString>

#include "licensing/hardware_info.h"

class QSettings;

namespace LLUtil {

const int LATEST_HWID_VERSION = 5;

const QnHardwareInfo &getHardwareInfo();

void initHardwareId(QSettings *settings);
QString getLatestHardwareId();
QStringList getAllHardwareIds();
int hardwareIdVersion(const QString& hardwareId);

}

#endif // _HARDWARE_ID_H_
