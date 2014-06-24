#include "update_utils.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QDir>
#include <QtCore/QCryptographicHash>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace {

const QString infoEntryName = lit("update.json");
const char passwordChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
const int passwordLength = 6;
const int readBufferSize = 1024 * 16;

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

    if (!info.contains(lit("executable")))
        return false;

    if (version)
        *version = locVersion;
    if (sysInfo)
        *sysInfo = locSysInfo;
    if (isClient)
        *isClient = info.value(lit("client")).toBool();

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

bool extractZipArchive(QuaZip *zip, const QDir &dir) {
    if (!dir.exists())
        return false;

    QuaZipFile file(zip);
    for (bool more = zip->goToFirstFile(); more; more = zip->goToNextFile()) {
        QuaZipFileInfo info;
        zip->getCurrentFileInfo(&info);

        QFileInfo fileInfo(info.name);
        QString path = fileInfo.path();

        if (!path.isEmpty() && !dir.exists(path) && !dir.mkpath(path))
            return false;

        if (!info.name.endsWith(lit("/"))) {
            QFile destFile(dir.absoluteFilePath(info.name));
            if (!destFile.open(QFile::WriteOnly))
                return false;

            if (!file.open(QuaZipFile::ReadOnly))
                return false;

            QByteArray buf(readBufferSize, 0);
            while (file.bytesAvailable()) {
                qint64 read = file.read(buf.data(), readBufferSize);
                if (read != destFile.write(buf.data(), read)) {
                    file.close();
                    return false;
                }
            }
            destFile.close();
            file.close();

            destFile.setPermissions(info.getPermissions());
        }
    }
    return zip->getZipError() == UNZ_OK;
}


bool extractZipArchive(const QString &zipFileName, const QDir &dir) {
    QuaZip zip(zipFileName);
    if (!zip.open(QuaZip::mdUnzip))
        return false;

    return extractZipArchive(&zip, dir);
}
