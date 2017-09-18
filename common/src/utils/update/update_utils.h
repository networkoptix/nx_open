#pragma once

#include <QtCore/QString>

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>

class QIODevice;
class QuaZip;
class QDir;

bool verifyUpdatePackage(
    const QString& fileName,
    QnSoftwareVersion* version = nullptr,
    QnSystemInformation* sysInfo = nullptr,
    QString* cloudHost = nullptr,
    bool* isClient = nullptr);
bool verifyUpdatePackage(
    QIODevice* device,
    QnSoftwareVersion* version = nullptr,
    QnSystemInformation* sysInfo = nullptr,
    QString* cloudHost = nullptr,
    bool* isClient = nullptr);
QString passwordForBuild(const QString& build);
QDir updatesCacheDir();
QString updateFilePath(const QString &fileName);
QString makeMd5(const QString &fileName);
QString makeMd5(QIODevice *device);
void clearUpdatesCache(const QnSoftwareVersion &versionToKeep);
