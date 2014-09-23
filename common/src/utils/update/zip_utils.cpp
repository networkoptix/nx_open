#include "zip_utils.h"

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace {
    const int readBufferSize = 1024 * 16;
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
    if (!m_dir.exists()) {
        finish(WrongDir);
        return;
    }

    if (!m_zip->open(QuaZip::mdUnzip)) {
        finish(BrokenZip);
        return;
    }

    QuaZipFile file(m_zip);
    for (bool more = m_zip->goToFirstFile(); more && !m_needStop; more = m_zip->goToNextFile()) {
        QuaZipFileInfo info;
        m_zip->getCurrentFileInfo(&info);

        QFileInfo fileInfo(info.name);
        QString path = fileInfo.path();

        if (!path.isEmpty() && !m_dir.exists(path) && !m_dir.mkpath(path)) {
            finish(OtherError);
            return;
        }

        if (!info.name.endsWith(lit("/"))) {
            QFile destFile(m_dir.absoluteFilePath(info.name));
            if (!destFile.open(QFile::WriteOnly)) {
                finish(CantOpenFile);
                return;
            }

            if (!file.open(QuaZipFile::ReadOnly)) {
                finish(BrokenZip);
                return;
            }

            QByteArray buf(readBufferSize, 0);
            while (file.bytesAvailable() && !m_needStop) {
                qint64 read = file.read(buf.data(), readBufferSize);
                if (read != destFile.write(buf.data(), read)) {
                    file.close();
                    finish(NoFreeSpace);
                    return;
                }
            }
            destFile.close();
            file.close();

            destFile.setPermissions(info.getPermissions());
        }
    }

    if (m_needStop) {
        finish(Stopped);
        return;
    }

    finish(m_zip->getZipError() == UNZ_OK ? Ok : BrokenZip);
}

void QnZipExtractor::finish(QnZipExtractor::Error error) {
    m_lastError = error;
    emit finished(error);
}
