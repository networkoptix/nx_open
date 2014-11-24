#include "update_utils.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QDir>
#include <QtCore/QCryptographicHash>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <utils/common/app_info.h>

namespace {

const QString infoEntryName = lit("update.json");
const char passwordChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
const int passwordLength = 6;
const int readBufferSize = 1024 * 16;
const QString updatesCacheDirName = QnAppInfo::productNameShort() + lit("_updates");

bool verifyUpdatePackageInternal(QuaZipFile *infoFile, QnSoftwareVersion *version = 0, QnSystemInformation *sysInfo = 0, bool *isClient = 0) {
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

    bool client = info.value(lit("client")).toBool();
    if (!client && !info.contains(lit("executable")))
        return false;

    if (version)
        *version = locVersion;
    if (sysInfo)
        *sysInfo = locSysInfo;
    if (isClient)
        *isClient = client;

    return true;
}

} // anonymous namespace


bool verifyUpdatePackage(const QString &fileName, QnSoftwareVersion *version, QnSystemInformation *sysInfo, bool *isClient) {
    QuaZipFile infoFile(fileName, infoEntryName);
    return verifyUpdatePackageInternal(&infoFile, version, sysInfo, isClient);
}


bool verifyUpdatePackage(QIODevice *device, QnSoftwareVersion *version, QnSystemInformation *sysInfo, bool *isClient) {
    QuaZip zip(device);
    if (!zip.open(QuaZip::mdUnzip))
        return false;

    zip.setCurrentFile(infoEntryName);
    QuaZipFile infoFile(&zip);
    return verifyUpdatePackageInternal(&infoFile, version, sysInfo, isClient);
}

QDir updatesCacheDir() {
    QDir dir = QDir::temp();
    if (!dir.exists(updatesCacheDirName))
        dir.mkdir(updatesCacheDirName);
    dir.cd(updatesCacheDirName);
    return dir;
}

QString updateFilePath(const QString &fileName) {
    return updatesCacheDir().absoluteFilePath(fileName);
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

QString passwordForBuild(unsigned buildNumber) {
    unsigned seed1 = buildNumber;
    unsigned seed2 = (buildNumber + 13) * 179;
    unsigned seed3 = buildNumber << 16;
    unsigned seed4 = ((buildNumber + 179) * 13) << 16;
    unsigned seed = seed1 ^ seed2 ^ seed3 ^ seed4;

    QString password;
    const int charsCount = sizeof(passwordChars) - 1;
    for (int i = 0; i < passwordLength; i++) {
        password += QChar::fromLatin1(passwordChars[seed % charsCount]);
        seed /= charsCount;
    }
    return password;
}

void clearUpdatesCache(const QnSoftwareVersion &versionToKeep) {
    QDir dir = updatesCacheDir();
    QStringList entries = dir.entryList(QDir::Files);
    for (const QString &fileName: entries) {
        QnSoftwareVersion version;
        if (verifyUpdatePackage(dir.absoluteFilePath(fileName), &version) && version == versionToKeep)
            continue;

        dir.remove(fileName);
    }
}
