#include "server_update_tool.h"

#include <QtCore/QDir>
#include <QtCore/QBuffer>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace {

    const QString updatesDirSuffix = lit("mediaserver/updates");
    const int readBufferSize = 1024 * 16;

    QDir getUpdatesDir() {
        QDir dir = QDir::temp();
        if (!dir.exists(updatesDirSuffix))
            dir.mkpath(updatesDirSuffix);
        dir.cd(updatesDirSuffix);
        return dir;
    }

    QDir getUpdateDir(const QString &updateId) {
        QDir dir = getUpdatesDir();
        if (!dir.exists(updateId))
            dir.mkdir(updateId);
        dir.cd(updateId);
        return dir;
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
            QString name = fileInfo.fileName();

            if (!path.isEmpty() && !dir.exists(path) && !dir.mkpath(path))
                return false;

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
        return zip->getZipError() == UNZ_OK;
    }

} // anonymous namespace

QnServerUpdateTool *QnServerUpdateTool::m_instance = NULL;

QnServerUpdateTool::QnServerUpdateTool() {
}

QnServerUpdateTool *QnServerUpdateTool::instance() {
    if (!m_instance)
        m_instance = new QnServerUpdateTool();

    return m_instance;
}

bool QnServerUpdateTool::addUpdateFile(const QString &updateId, const QByteArray &data) {
    QBuffer buffer(const_cast<QByteArray*>(&data)); // we're goint to read data, so const_cast is ok here
    QuaZip zip(&buffer);
    if (!zip.open(QuaZip::mdUnzip))
        return false;

    bool ok = extractZipArchive(&zip, getUpdateDir(updateId));

    zip.close();

    return ok;
}

bool QnServerUpdateTool::installUpdate(const QString &updateId) {
    return true;
}
