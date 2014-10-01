#ifndef UPDATE_UTILS_H
#define UPDATE_UTILS_H

#include <QtCore/QString>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

class QIODevice;
class QuaZip;
class QDir;

bool verifyUpdatePackage(const QString &fileName, QnSoftwareVersion *version = 0, QnSystemInformation *sysInfo = 0, bool *isClient = 0);
bool verifyUpdatePackage(QIODevice *device, QnSoftwareVersion *version = 0, QnSystemInformation *sysInfo = 0, bool *isClient = 0);
QString passwordForBuild(unsigned buildNumber);
QDir updatesCacheDir();
QString updateFilePath(const QString &fileName);
QString makeMd5(const QString &fileName);
QString makeMd5(QIODevice *device);
void clearUpdatesCache(const QnSoftwareVersion &versionToKeep);

#endif // UPDATE_UTILS_H
