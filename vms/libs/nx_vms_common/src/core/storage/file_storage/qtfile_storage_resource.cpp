// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qtfile_storage_resource.h"

#include <QtCore/QDir>

#include <nx/utils/random.h>
#include <utils/common/util.h>

namespace {

QString removeProtocolPrefix(const QStringView& url)
{
    const int prefixPos = url.indexOf(QLatin1String("://"));
    if (prefixPos == -1)
        return url.toString();

    const auto result = url.mid(prefixPos + 3);
    return (result.startsWith('/') ? result.mid(1) : result).toString();
}

} // namespace

QIODevice* QnQtFileStorageResource::openInternal(const QString& fileName, QIODevice::OpenMode openMode)
{
    const QString cleanFileName = removeProtocolPrefix(fileName);

    auto rez = new QFile(cleanFileName);
    if (!rez->open(openMode))
    {
        delete rez;
        return nullptr;
    }
    return rez;
}

int QnQtFileStorageResource::getCapabilities() const
{
    return ListFile | ReadFile;
}

QnQtFileStorageResource::QnQtFileStorageResource():
    base_type()
{
}

bool QnQtFileStorageResource::removeFile(const QString& url)
{
    return QFile::remove(removeProtocolPrefix(url));
}

bool QnQtFileStorageResource::renameFile(const QString& oldName, const QString& newName)
{
    return QFile::rename(oldName, newName);
}

bool QnQtFileStorageResource::removeDir(const QString& url)
{
    QDir dir(removeProtocolPrefix(url));
    QList<QFileInfo> list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi: list)
        removeFile(fi.absoluteFilePath());
    return true;
}

bool QnQtFileStorageResource::isDirExists(const QString& url)
{
    QDir d(url);
    return d.exists(removeProtocolPrefix(url));
}

bool QnQtFileStorageResource::isFileExists(const QString& url)
{
    return QFile::exists(removeProtocolPrefix(url));
}

qint64 QnQtFileStorageResource::getFreeSpace()
{
    NX_ASSERT("false", "This is a server-only method");
    return 0;
}

qint64 QnQtFileStorageResource::getTotalSpace() const
{
    NX_ASSERT("false", "This is a server-only method");
    return 0;
}

QnAbstractStorageResource::FileInfoList QnQtFileStorageResource::getFileList(
    const QString& dirName)
{
    QDir dir;
    if (dir.cd(dirName))
        return FIListFromQFIList(dir.entryInfoList(QDir::Files));

    return FileInfoList();
}

qint64 QnQtFileStorageResource::getFileSize(const QString& /*url*/) const
{
    NX_ASSERT("false", "This is a server-only method");
    return 0;
}

Qn::StorageInitResult QnQtFileStorageResource::initOrUpdate()
{
    const QString tmpDir = closeDirPath(getUrl())
        + "tmp"
        + QString::number(nx::utils::random::number<uint>());

    QDir dir(tmpDir);
    if (dir.exists() && dir.removeRecursively())
        return Qn::StorageInit_Ok;

    if (dir.mkpath(tmpDir) && dir.rmdir(tmpDir))
        return Qn::StorageInit_Ok;

    return Qn::StorageInit_WrongPath;
}

QnStorageResource* QnQtFileStorageResource::instance(const QString&)
{
    return new QnQtFileStorageResource();
}
