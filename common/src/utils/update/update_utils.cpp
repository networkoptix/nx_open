#include "update_utils.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QDir>
#include <QtCore/QCryptographicHash>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace {

const QString infoEntryName = lit("update.json");


bool verifyUpdatePackageInternal(QuaZipFile *infoFile, QnSoftwareVersion *version = 0, QnSystemInformation *sysInfo = 0) {
    if (!infoFile->open(QuaZipFile::ReadOnly))
        return false;

    QVariantMap info = QJsonDocument::fromJson(infoFile->readAll()).toVariant().toMap();
    if (info.isEmpty())
        return false;

    QnSystemInformation locSysInfo(info.value(lit("platform")).toString(), info.value(lit("arch")).toString(), info.value(lit("modification")).toString());
    if (!locSysInfo.isValid())
        return false;

    QnSoftwareVersion locVersion(info.value(lit("version")).toString());
    if (locVersion.isNull())
        return false;

    if (!info.contains(lit("executable")))
        return false;

    if (version)
        *version = locVersion;
    if (sysInfo)
        *sysInfo = locSysInfo;

    return true;
}

} // anonymous namespace


bool verifyUpdatePackage(const QString &fileName, QnSoftwareVersion *version, QnSystemInformation *sysInfo) {
    QuaZipFile infoFile(fileName, infoEntryName);
    return verifyUpdatePackageInternal(&infoFile, version, sysInfo);
}


bool verifyUpdatePackage(QIODevice *device, QnSoftwareVersion *version, QnSystemInformation *sysInfo) {
    QuaZip zip(device);
    if (!zip.open(QuaZip::mdUnzip))
        return false;

    zip.setCurrentFile(infoEntryName);
    QuaZipFile infoFile(&zip);
    return verifyUpdatePackageInternal(&infoFile, version, sysInfo);
}

QString updateFilePath(const QString &updatesDirPath, const QString &fileName) {
    QDir dir = QDir::temp();
    if (!dir.exists(updatesDirPath))
        dir.mkdir(updatesDirPath);
    dir.cd(updatesDirPath);
    return dir.absoluteFilePath(fileName);
}


QString makeMd5(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return QString();

    return makeMd5(&file);
}


QString makeMd5(QIODevice *device) {
    if (!device->isOpen() && !device->open(QIODevice::ReadOnly))
        return QString();

    if (!device->isSequential())
        device->seek(0);

    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!hash.addData(device))
        return QString();

    return QString::fromLatin1(hash.result().toHex());
}
