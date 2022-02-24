// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_storage_resource.h"

#include <nx/utils/url.h>
#include <utils/common/util.h>

QnAbstractStorageResource::FileInfo::FileInfo(const QString& uri, qint64 size, bool isDir):
    m_fpath(uri),
    m_size(size),
    m_isDir(isDir)
{
}

QnAbstractStorageResource::FileInfo::FileInfo(const QFileInfo& qfi):
    m_qFileInfo(new QFileInfo(qfi))
{
}

bool QnAbstractStorageResource::FileInfo::operator==(
    const QnAbstractStorageResource::FileInfo& other) const
{
    return absoluteFilePath() == other.absoluteFilePath();
}

bool QnAbstractStorageResource::FileInfo::operator!=(
    const QnAbstractStorageResource::FileInfo& other) const
{
    return !operator==(other);
}

QnAbstractStorageResource::FileInfo QnAbstractStorageResource::FileInfo::fromQFileInfo(
    const QFileInfo& fi)
{
    return FileInfo(fi);
}

bool QnAbstractStorageResource::FileInfo::isDir() const
{
    return m_qFileInfo ? m_qFileInfo->isDir() : m_isDir;
}

QString QnAbstractStorageResource::FileInfo::absoluteFilePath() const
{
    return m_qFileInfo ? m_qFileInfo->absoluteFilePath() : m_fpath;
}

QString QnAbstractStorageResource::FileInfo::absoluteDirPath() const
{
    if (m_qFileInfo)
        return m_qFileInfo->absoluteDir().path();

    if (m_isDir)
        return m_fpath;

    const auto lastSeparatorIndex = m_fpath.lastIndexOf(getPathSeparator(m_fpath));
    if (lastSeparatorIndex == -1)
        return m_fpath;

    return m_fpath.left(lastSeparatorIndex);
}

QString QnAbstractStorageResource::FileInfo::fileName() const
{
    if (m_qFileInfo)
        return m_qFileInfo->fileName();

    const auto lastSeparatorIndex = m_fpath.lastIndexOf(getPathSeparator(m_fpath));
    if (lastSeparatorIndex == -1)
        return m_fpath;

    return m_fpath.mid(lastSeparatorIndex + 1);
}

QString QnAbstractStorageResource::FileInfo::baseName() const
{
    if (m_qFileInfo)
        return m_qFileInfo->baseName();

    const auto fileName = this->fileName();
    const int dotIndex = fileName.indexOf('.');
    if (dotIndex == -1)
        return fileName;

    return fileName.left(dotIndex);
}

qint64 QnAbstractStorageResource::FileInfo::size() const
{
    return m_qFileInfo ? m_qFileInfo->size() : m_size;
}

QDateTime QnAbstractStorageResource::FileInfo::created() const
{
    return m_qFileInfo ? m_qFileInfo->birthTime() : QDateTime();
}

QString QnAbstractStorageResource::FileInfo::extension() const
{
    const auto fileName = this->fileName();
    const int lastDotIndex = fileName.lastIndexOf('.');
    if (lastDotIndex == -1)
        return "";

    return fileName.mid(lastDotIndex + 1);
}

QString QnAbstractStorageResource::FileInfo::toString() const
{
    return nx::utils::url::hidePassword(absoluteFilePath());
}

void QnAbstractStorageResource::FileInfo::setPath(const QString& path)
{
    m_fpath = path;
}

QnAbstractStorageResource::FileInfoList QnAbstractStorageResource::FIListFromQFIList(
    const QFileInfoList& list)
{
    FileInfoList ret;
    for (const auto& info: list)
        ret.append(FileInfo(info));
    return ret;
}
