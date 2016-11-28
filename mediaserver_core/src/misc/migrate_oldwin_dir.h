#pragma once

#include "QtCore/QString"

#ifdef Q_OS_WIN32

namespace misc {

void migrateFilesFromWindowsOldDir(const QString& currentDataDir);

} // misc

#endif // Q_OS_WIN32
