#pragma once

#include <stdexcept>
#include <string>

#include <vector>
#include <QtCore/QString>

class QnMediaServerModule;
struct QnHardwareInfo;

namespace LLUtil {

const int LATEST_HWID_VERSION = 5;

// This function should be called once before
// any other calls to the hardware id library
// It uses settings to store/retrive MAC addess hardware id is bound to.
void initHardwareId(QnMediaServerModule* serverModule);

const QnHardwareInfo &getHardwareInfo();
QString getLatestHardwareId();
QStringList getAllHardwareIds();

}
