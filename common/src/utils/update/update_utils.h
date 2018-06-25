#pragma once

#include <QtCore/QString>

#include <utils/common/system_information.h>

#include <nx/utils/software_version.h>

class QIODevice;
class QuaZip;
class QDir;

bool verifyUpdatePackage(
    const QString& fileName,
    nx::utils::SoftwareVersion* version = nullptr,
    QnSystemInformation* sysInfo = nullptr,
    QString* cloudHost = nullptr,
    bool* isClient = nullptr);

bool verifyUpdatePackage(
    QIODevice* device,
    nx::utils::SoftwareVersion* version = nullptr,
    QnSystemInformation* sysInfo = nullptr,
    QString* cloudHost = nullptr,
    bool* isClient = nullptr);

QString passwordForBuild(const QString& build);
QDir updatesCacheDir();
QString updateFilePath(const QString& fileName);
QString makeMd5(const QString& fileName);
QString makeMd5(QIODevice* device);
void clearUpdatesCache(const nx::utils::SoftwareVersion& versionToKeep);
