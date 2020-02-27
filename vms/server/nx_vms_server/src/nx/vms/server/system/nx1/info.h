#pragma once

#include <QtCore/QString>

class QnMediaServerModule;

namespace Nx1 {

QString getMac();
QString getSerial();
bool isBootedFromSD(QnMediaServerModule* serverModule);

}

