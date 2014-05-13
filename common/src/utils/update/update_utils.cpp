#include "update_utils.h"

#include <QtCore/QJsonDocument>

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

    QnSystemInformation locSysInfo(info.value(lit("platform")).toString(), info.value(lit("arch")).toString());
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
    zip.setCurrentFile(infoEntryName);
    QuaZipFile infoFile(&zip);
    return verifyUpdatePackageInternal(&infoFile, version, sysInfo);
}
