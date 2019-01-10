#pragma once

#include <QtCore/QString>

namespace nx::vms::server { class RootFileSystem; }

namespace Nx1 {

QString getMac();
QString getSerial();
bool isBootedFromSD(nx::vms::server::RootFileSystem* footFs);

}

