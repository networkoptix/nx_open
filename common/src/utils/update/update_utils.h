#pragma once

#include <QtCore/QString>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/system_information.h>

class QIODevice;
class QuaZip;
class QDir;

bool verifyUpdatePackage(
    const QString& fileName,
    nx::utils::SoftwareVersion* version = nullptr,
    nx::vms::api::SystemInformation* sysInfo = nullptr,
    QString* cloudHost = nullptr,
    bool* isClient = nullptr);

bool verifyUpdatePackage(
    QIODevice* device,
    nx::utils::SoftwareVersion* version = nullptr,
    nx::vms::api::SystemInformation* sysInfo = nullptr,
    QString* cloudHost = nullptr,
    bool* isClient = nullptr);

QString passwordForBuild(const QString& build);
QDir updatesCacheDir();
QString updateFilePath(const QString& fileName);
QString makeMd5(const QString& fileName);
QString makeMd5(QIODevice* device);
void clearUpdatesCache(const nx::utils::SoftwareVersion& versionToKeep);
