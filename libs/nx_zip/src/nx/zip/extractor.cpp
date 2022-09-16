// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "extractor.h"

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace {

const int kReadBufferSizeBytes = 1024 * 16;
const int maxSymlinkLength = kReadBufferSizeBytes;

bool isSymlink(const QuaZipFileInfo64& info)
{
    /* According to zip format specifications the higher 4 bits contain file type. Symlink is 0xA.
     */
    return (info.externalAttr >> 28) == 0xA;
}

} // namespace

namespace nx::zip {

Extractor::Extractor(const QString& fileName, const QDir& targetDir):
    m_dir(targetDir),
    m_zip(new QuaZip(fileName))
{
    qRegisterMetaType<Error>();
}

Extractor::~Extractor()
{
    stop();
}

QString Extractor::errorToString(Extractor::Error error)
{
    switch (error)
    {
        case Error::BrokenZip:
            return tr("Zip file is corrupted.");
        case Error::WrongDir:
            return tr("Could not find target dir.");
        case Error::CantOpenFile:
            return tr("Could not open file for writing.");
        case Error::NoFreeSpace:
            return tr("There is no free space on the disk.");
        case Error::OtherError:
            return tr("Unknown error.");
        case Error::Stopped:
            return tr("Extraction was cancelled.");
        case Error::Busy:
            return tr("Extractor is busy.");
        default:
            return QString();
    }
}

Extractor::Error Extractor::error() const
{
    return m_lastError;
}

QDir Extractor::dir() const
{
    return m_dir;
}

void Extractor::run()
{
    Error error = extractZip();
    m_lastError = error;
    emit finished(error);
}

Extractor::Error Extractor::extractZip()
{
    if (!m_dir.exists())
        return WrongDir;

    if (!m_zip->open(QuaZip::mdUnzip))
        return BrokenZip;

    m_extracted = 0;
    QList<QPair<QString, QString>> symlinks;

    QuaZipFile file(m_zip.get());
    for (bool more = m_zip->goToFirstFile(); more && !m_needStop; more = m_zip->goToNextFile())
    {
        QuaZipFileInfo64 info;
        m_zip->getCurrentFileInfo(&info);

        QFileInfo fileInfo(info.name);
        QString path = fileInfo.path();

        if (!path.isEmpty() && !m_dir.exists(path) && !m_dir.mkpath(path))
            return OtherError;

        if (isSymlink(info))
        {
            if (!file.open(QuaZipFile::ReadOnly))
                return BrokenZip;

            if (file.size() > maxSymlinkLength)
                return OtherError;

            QString linkTarget = QString::fromLatin1(file.readAll());
            file.close();
            symlinks.append(QPair<QString, QString>(info.name, linkTarget));

            continue;
        }

        if (!info.name.endsWith("/"))
        {
            QFile destFile(m_dir.absoluteFilePath(info.name));
            if (!destFile.open(QFile::WriteOnly))
                return CantOpenFile;

            if (!file.open(QuaZipFile::ReadOnly))
                return BrokenZip;

            QByteArray buf(kReadBufferSizeBytes, 0);
            while (file.bytesAvailable() && !m_needStop)
            {
                qint64 read = file.read(buf.data(), kReadBufferSizeBytes);
                if (read != destFile.write(buf.data(), read))
                {
                    file.close();
                    return NoFreeSpace;
                }
                m_extracted += read;
            }

            auto permissions = info.getPermissions();
            permissions |= QFile::ReadOwner;
            permissions |= QFile::ReadUser;
            permissions |= QFile::ReadGroup;
            destFile.setPermissions(permissions);
            destFile.close();
            file.close();
        }

        if (m_needStop)
            return Stopped;
    }

    for (const QPair<QString, QString>& symlink: symlinks)
    {
        QString link = m_dir.absoluteFilePath(symlink.first);
        QString target = symlink.second;
        if (!QFile::link(target, link))
            return OtherError;
    }

    if (m_needStop)
        return Stopped;

    m_zip->close();

    return m_zip->getZipError() == UNZ_OK ? Ok : BrokenZip;
}

QStringList Extractor::fileList()
{
    if (!m_zip->open(QuaZip::mdUnzip))
        return QStringList();

    QStringList result = m_zip->getFileNameList();

    m_zip->close();

    return result;
}

Extractor::Error Extractor::tryOpen()
{
    if (!m_dir.exists())
        return WrongDir;

    if (!m_zip->open(QuaZip::mdUnzip))
        return BrokenZip;

    m_zip->close();

    return Ok;
}

void Extractor::createEmptyZipFile()
{
    if (m_zip->open(QuaZip::mdCreate))
        m_zip->close();
}

qint64 Extractor::estimateUnpackedSize() const
{
    int64_t result = 0;
    if (!m_zip->open(QuaZip::mdUnzip))
        return -1;

    QList<QuaZipFileInfo64> fileList = m_zip->getFileInfoList64();
    m_zip->close();

    for (const QuaZipFileInfo64& file: fileList)
        result += file.uncompressedSize;

    // It is quite hard to predict accurate size of unpacked data. This size will be
    // affected by the block size of a filesystem.
    auto constexpr kMultipler = 1.1;
    return static_cast<qint64>(result * kMultipler);
}

qint64 Extractor::bytesExtracted() const
{
    return m_extracted;
}

} // namespace nx::zip
