#include "zip_utils.h"

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace {
    const int readBufferSize = 1024 * 16;
    const int maxSymlinkLength = readBufferSize;

    bool isSymlink(const QuaZipFileInfo &info) {
        return (info.externalAttr >> 28) == 0xA;
    }
}

QnZipExtractor::QnZipExtractor(const QString &fileName, const QDir &targetDir) :
    m_dir(targetDir),
    m_zip(new QuaZip(fileName)),
    m_lastError(Ok)
{
}

QnZipExtractor::QnZipExtractor(QIODevice *ioDevice, const QDir &targetDir) :
    m_dir(targetDir),
    m_zip(new QuaZip(ioDevice)),
    m_lastError(Ok)
{
}

QnZipExtractor::~QnZipExtractor() {
    stop();
}

QString QnZipExtractor::errorToString(QnZipExtractor::Error error) {
    switch (error) {
    case BrokenZip:
        return lit("Zip file is brocken.");
    case WrongDir:
        return lit("Could not find target dir.");
    case CantOpenFile:
        return lit("Could not open file for writing.");
    case NoFreeSpace:
        return lit("There is no free space on the disk.");
    case OtherError:
        return lit("Unknown error.");
    case Stopped:
        return lit("Extraction was canceled.");
    default:
        return QString();
    }
}

QnZipExtractor::Error QnZipExtractor::error() const {
    return m_lastError;
}

QDir QnZipExtractor::dir() const {
    return m_dir;
}

void QnZipExtractor::run() {
    Error error = extractZip();
    m_lastError = error;
    emit finished(error);
}

QnZipExtractor::Error QnZipExtractor::extractZip() {
    if (!m_dir.exists())
        return WrongDir;

    if (!m_zip->open(QuaZip::mdUnzip))
        return BrokenZip;

    QList<QPair<QString, QString>> symlinks;

    QuaZipFile file(m_zip);
    for (bool more = m_zip->goToFirstFile(); more && !m_needStop; more = m_zip->goToNextFile()) {
        QuaZipFileInfo info;
        m_zip->getCurrentFileInfo(&info);

        QFileInfo fileInfo(info.name);
        QString path = fileInfo.path();

        if (!path.isEmpty() && !m_dir.exists(path) && !m_dir.mkpath(path))
            return OtherError;

        if (isSymlink(info)) {
            if (!file.open(QuaZipFile::ReadOnly))
                return BrokenZip;

            if (file.size() > maxSymlinkLength)
                return OtherError;

            QString linkTarget = QString::fromLatin1(file.readAll());
            file.close();
            symlinks.append(QPair<QString, QString>(info.name, linkTarget));

            continue;
        }

        if (!info.name.endsWith(lit("/"))) {
            QFile destFile(m_dir.absoluteFilePath(info.name));
            if (!destFile.open(QFile::WriteOnly))
                return CantOpenFile;

            if (!file.open(QuaZipFile::ReadOnly))
                return BrokenZip;

            QByteArray buf(readBufferSize, 0);
            while (file.bytesAvailable() && !m_needStop) {
                qint64 read = file.read(buf.data(), readBufferSize);
                if (read != destFile.write(buf.data(), read)) {
                    file.close();
                    return NoFreeSpace;
                }
            }
            destFile.close();
            file.close();

            destFile.setPermissions(info.getPermissions());
        }
    }

    for (const QPair<QString, QString> &symlink: symlinks) {
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

QStringList QnZipExtractor::fileList() {
    if (!m_zip->open(QuaZip::mdUnzip))
        return QStringList();

    QStringList result = m_zip->getFileNameList();

    m_zip->close();

    return result;
}
