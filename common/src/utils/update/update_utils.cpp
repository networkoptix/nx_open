#include "update_utils.h"

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

#include <QtCore/QJsonDocument>
#include <QtCore/QDir>
#include <QtCore/QCryptographicHash>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <utils/common/app_info.h>
#include <nx/fusion/model_functions.h>

namespace {

const QString infoEntryName = lit("update.json");
const char passwordChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
const int passwordLength = 6;
//const int readBufferSize = 1024 * 16;
const QString updatesCacheDirName = QnAppInfo::productNameShort() + lit("_updates");

struct UpdateFileData
{
    QnSoftwareVersion version;
    QString platform;
    QString arch;
    QString modification;
    QString cloudHost;
    QString executable;
    bool client = false;
    QnSoftwareVersion minimalVersion;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UpdateFileData, (json),
    (version)(platform)(arch)(modification)(cloudHost)(executable)(client)(minimalVersion))

bool verifyUpdatePackageInternal(
    QuaZipFile* infoFile,
    QnSoftwareVersion* version = nullptr,
    QnSystemInformation* sysInfo = nullptr,
    QString* cloudHost = nullptr,
    bool* isClient = nullptr)
{
    if (!infoFile->open(QuaZipFile::ReadOnly))
        return false;

    const auto data = infoFile->readAll();
    auto fileData = QJson::deserialized<UpdateFileData>(data);

    QnSystemInformation locSysInfo(fileData.platform, fileData.arch, fileData.modification);
    if (!locSysInfo.isValid())
        return false;

    if (!fileData.client && fileData.executable.isEmpty())
        return false;

    if (version)
        *version = fileData.version;
    if (sysInfo)
        *sysInfo = locSysInfo;
    if (cloudHost)
        *cloudHost = fileData.cloudHost;
    if (isClient)
        *isClient = fileData.client;

    return true;
}

} // anonymous namespace


bool verifyUpdatePackage(
    const QString& fileName,
    QnSoftwareVersion* version,
    QnSystemInformation* sysInfo,
    QString* cloudHost,
    bool* isClient)
{
    QuaZipFile infoFile(fileName, infoEntryName);
    return verifyUpdatePackageInternal(&infoFile, version, sysInfo, cloudHost, isClient);
}


bool verifyUpdatePackage(
    QIODevice* device,
    QnSoftwareVersion* version,
    QnSystemInformation* sysInfo,
    QString* cloudHost,
    bool* isClient)
{
    QuaZip zip(device);
    if (!zip.open(QuaZip::mdUnzip))
        return false;

    zip.setCurrentFile(infoEntryName);
    QuaZipFile infoFile(&zip);
    return verifyUpdatePackageInternal(&infoFile, version, sysInfo, cloudHost, isClient);
}

QDir updatesCacheDir()
{
    QDir dir = QDir::temp();
    if (!dir.exists(updatesCacheDirName))
        dir.mkdir(updatesCacheDirName);
    dir.cd(updatesCacheDirName);
    return dir;
}

QString updateFilePath(const QString& fileName)
{
    return updatesCacheDir().absoluteFilePath(fileName);
}

QString makeMd5(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return QString();

    return makeMd5(&file);
}


QString makeMd5(QIODevice* device)
{
    if (!device->isOpen() && !device->open(QIODevice::ReadOnly))
        return QString();

    if (!device->isSequential())
        device->seek(0);

    QCryptographicHash hash(QCryptographicHash::Md5);
    if (!hash.addData(device))
        return QString();

    return QString::fromLatin1(hash.result().toHex());
}

QString passwordForBuild(const QString& build)
{
    // Leaving only numbers from the build string, as it may be a changeset.
    QString buildNum;
    for (const auto c: build)
    {
        if (c.isDigit())
            buildNum.append(c);
    }

    unsigned buildNumber = unsigned(buildNum.toInt());

    unsigned seed1 = buildNumber;
    unsigned seed2 = (buildNumber + 13) * 179;
    unsigned seed3 = buildNumber << 16;
    unsigned seed4 = ((buildNumber + 179) * 13) << 16;
    unsigned seed = seed1 ^ seed2 ^ seed3 ^ seed4;

    QString password;
    const int charsCount = sizeof(passwordChars) - 1;
    for (int i = 0; i < passwordLength; i++)
    {
        password += QChar::fromLatin1(passwordChars[seed % charsCount]);
        seed /= charsCount;
    }
    return password;
}

void clearUpdatesCache(const QnSoftwareVersion& versionToKeep)
{
    QDir dir = updatesCacheDir();
    QStringList entries = dir.entryList(QDir::Files);
    for (const auto& fileName: entries)
    {
        QnSoftwareVersion version;
        if (verifyUpdatePackage(dir.absoluteFilePath(fileName), &version)
            && version == versionToKeep)
        {
            continue;
        }

        dir.remove(fileName);
    }
}

#endif
