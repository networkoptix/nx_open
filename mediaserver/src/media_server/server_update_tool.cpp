#include "server_update_tool.h"

#include <QtCore/QDir>
#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <media_server/settings.h>
#include <utils/common/log.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>

#include <version.h>

void stopServer(int signal);

namespace {

    const QString updatesDirSuffix = lit("mediaserver/updates");
    const QString updateInfoFileName = lit("update.json");
    const QString updateLogFileName = lit("update.log");
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

    QString getUpdateFilePath(const QString &updateId) {
        return getUpdatesDir().absoluteFilePath(updateId + lit(".zip"));
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

    bool initializeUpdateLog(const QString &targetVersion, QString *logFileName) {
        QString logDir = MSSettings::roSettings()->value(lit("logDir")).toString();
        if (logDir.isEmpty())
            return false;

        QString fileName = QDir(logDir).absoluteFilePath(updateLogFileName);
        QFile logFile(fileName);
        if (!logFile.open(QFile::Append))
            return false;

        QByteArray preface;
        preface.append("================================================================================\n");
        preface.append(QString(lit(" [%1] Starting system update:\n")).arg(QDateTime::currentDateTime().toString()));
        preface.append(QString(lit("    Current version: %1\n")).arg(lit(QN_APPLICATION_VERSION)));
        preface.append(QString(lit("    Target version: %1\n")).arg(targetVersion));
        preface.append("================================================================================\n");

        logFile.write(preface);
        logFile.close();

        *logFileName = fileName;
        return true;
    }

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }

} // anonymous namespace

QnServerUpdateTool *QnServerUpdateTool::m_instance = NULL;

QnServerUpdateTool::QnServerUpdateTool() {}

bool QnServerUpdateTool::processUpdate(const QString &updateId, QIODevice *ioDevice) {
    QuaZip zip(ioDevice);
    if (!zip.open(QuaZip::mdUnzip)) {
        cl_log.log("Could not open update package.", cl_logWARNING);
        return false;
    }

    QDir updateDir = getUpdateDir(updateId);

    bool ok = extractZipArchive(&zip, updateDir);

    zip.close();

    if (ok)
        cl_log.log("Update package has been extracted to ", updateDir.path(), cl_logINFO);
    else
        cl_log.log("Could not extract update package to ", updateDir.path(), cl_logWARNING);

    return ok;
}

QnServerUpdateTool *QnServerUpdateTool::instance() {
    if (!m_instance)
        m_instance = new QnServerUpdateTool();

    return m_instance;
}

bool QnServerUpdateTool::addUpdateFile(const QString &updateId, const QByteArray &data) {
    QBuffer buffer(const_cast<QByteArray*>(&data)); // we're goint to read data, so const_cast is ok here
    return processUpdate(updateId, &buffer);
}

bool QnServerUpdateTool::addUpdateFileChunk(const QString &updateId, const QByteArray &data, qint64 offset) {
    if (m_bannedUpdates.contains(updateId))
        return false;

    TransferInfo *transfer = m_transfers[updateId];
    if (!transfer) {
        transfer = new TransferInfo;
        transfer->file.reset(new QFile(getUpdateFilePath(updateId)));
        if (!transfer->file->open(QFile::WriteOnly)) {
            cl_log.log("Could not save update to ", transfer->file->fileName(), cl_logERROR);
            m_bannedUpdates.insert(updateId);
            delete transfer;
            return false;
        }
        m_transfers[updateId] = transfer;
    }

    if (data.isEmpty()) { // it means we're just got the size of the file
        transfer->length = offset;
    } else {
        transfer->file->seek(offset);
        transfer->file->write(data);
        transfer->addChunk(offset, offset + data.size());

        connection2()->getUpdatesManager()->sendUpdateUploadedResponce(updateId, qnCommon->moduleGUID(), offset,
                                                                       this, [this](int reqID, ec2::ErrorCode errorCode) {});
    }

    if (transfer->isComplete()) {
        transfer->file->close();
        m_transfers.remove(updateId);

        bool ok = processUpdate(updateId, transfer->file.data());

        if (ok)
            connection2()->getUpdatesManager()->sendUpdateUploadedResponce(updateId, qnCommon->moduleGUID(), -1,
                                                                           this, [this](int reqID, ec2::ErrorCode errorCode) {});

        delete transfer;
        return ok;
    }

    return true;
}

bool QnServerUpdateTool::installUpdate(const QString &updateId) {
    cl_log.log("Starting update to ", updateId, cl_logINFO);

    QDir updateDir = getUpdateDir(updateId);
    if (!updateDir.exists()) {
        cl_log.log("Update dir does not exist: ", updateDir.path(), cl_logERROR);
        return false;
    }

    QFile updateInfoFile(updateDir.absoluteFilePath(updateInfoFileName));
    if (!updateInfoFile.open(QFile::ReadOnly)) {
        cl_log.log("Could not open update information file: ", updateInfoFile.fileName(), cl_logERROR);
        return false;
    }

    QVariantMap map = QJsonDocument::fromJson(updateInfoFile.readAll()).toVariant().toMap();
    updateInfoFile.close();

    QString executable = map.value(lit("executable")).toString();
    if (executable.isEmpty()) {
        cl_log.log("Wrong update information file: ", updateInfoFile.fileName(), cl_logERROR);
        return false;
    }

    if (map.value(lit("platform")) != lit(QN_APPLICATION_PLATFORM)) {
        cl_log.log("Wrong update information file: ", updateInfoFile.fileName(), cl_logERROR);
        return false;
    }
    if (map.value(lit("arch")) != lit(QN_APPLICATION_ARCH)) {
        cl_log.log("Wrong update information file: ", updateInfoFile.fileName(), cl_logERROR);
        return false;
    }

    QString currentDir = QDir::currentPath();
    QDir::setCurrent(updateDir.absolutePath());

    QProcess process;

    QString logFileName;
    if (initializeUpdateLog(updateId, &logFileName))
        process.setStandardOutputFile(logFileName, QFile::Append);
    else
        cl_log.log("Could not create or open update log file.", cl_logWARNING);

    process.start(updateDir.absoluteFilePath(executable));
    process.waitForFinished(10 * 60 * 1000); // wait for ten minutes

    // possibly we'll be killed by updater and wont get here

    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        cl_log.log("Update finished successfully but server has not been restarted", cl_logWARNING);
        stopServer(0); // TODO: #dklychkov make other way to stop server
    } else {
        cl_log.log("Update failed. See update log for details: ", logFileName, cl_logERROR);
    }

    QDir::setCurrent(currentDir);

    return true;
}


QnServerUpdateTool::TransferInfo::TransferInfo() : length(-1) {}

QnServerUpdateTool::TransferInfo::~TransferInfo() {}

void QnServerUpdateTool::TransferInfo::addChunk(qint64 start, qint64 end) {
    int i = 0;
    for ( ; i < chunks.size() && chunks[i].start <= start; i++) {}

    if (i > 0)
        i--;

    if (i < chunks.size()) {
        Chunk *chunk = &(chunks[i]);

        if (start == chunk->start)
            chunk->end = qMax(chunk->end, end);
        else if (start == chunk->end)
            chunk->end = end;
        else if (!(start >= chunk->start && end <= chunk->end))
            chunks.insert(++i, Chunk(start, end));
    } else {
        chunks.append(Chunk(start, end));
    }

    if (i < chunks.size() - 1) {
        if (chunks[i].end >= chunks[i + 1].start) {
            chunks[i].end = chunks[i + 1].end;
            chunks.removeAt(i + 1);
        }
    }
}

bool QnServerUpdateTool::TransferInfo::isComplete() const {
    return length != -1 && chunks.size() == 1 && chunks.first().start == 0 && chunks.first().end == length;
}
