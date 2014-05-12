#include "server_update_tool.h"

#include <QtCore/QDir>
#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include <utils/common/log.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>

#include <version.h>

namespace {

    const QString updatesDirSuffix = lit("mediaserver/updates");
    const QString updateInfoFileName = lit("update.json");
    const QString updateLogFileName = lit("update.log");
    const int readBufferSize = 1024 * 16;
    const int replyDelay = 200;

    QDir getUpdatesDir() {
        QDir dir = QDir::temp();
        if (!dir.exists(updatesDirSuffix))
            dir.mkpath(updatesDirSuffix);
        dir.cd(updatesDirSuffix);
        return dir;
    }

    QDir getUpdateDir(const QString &updateId) {
        QString id = updateId.mid(1, updateId.length() - 2);
        QDir dir = getUpdatesDir();
        if (!dir.exists(id))
            dir.mkdir(id);
        dir.cd(id);
        return dir;
    }

    QString getUpdateFilePath(const QString &updateId) {
        QString id = updateId.mid(1, updateId.length() - 2);
        return getUpdatesDir().absoluteFilePath(id + lit(".zip"));
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
        QString logDir = MSSettings::roSettings()->value(lit("logDir"), getDataDirectory() + lit("/log/")).toString();
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

QnServerUpdateTool::QnServerUpdateTool() : m_length(-1), m_replyTime(0) {}

QnServerUpdateTool::~QnServerUpdateTool() {}

QnServerUpdateTool *QnServerUpdateTool::instance() {
    if (!m_instance)
        m_instance = new QnServerUpdateTool();

    return m_instance;
}

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

void QnServerUpdateTool::sendReply(int code) {
    if (code == 0) {
        qint64 currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
        if (currentTime - m_replyTime < replyDelay)
            return;

        m_replyTime = currentTime;
    }

    connection2()->getUpdatesManager()->sendUpdateUploadResponce(m_updateId, qnCommon->moduleGUID(), code == 0 ? m_chunks.size() : code,
                                                                 this, [this](int, ec2::ErrorCode) {});
}

bool QnServerUpdateTool::addUpdateFile(const QString &updateId, const QByteArray &data) {
    clearUpdatesLocation();
    QBuffer buffer(const_cast<QByteArray*>(&data)); // we're goint to read data, so const_cast is ok here
    return processUpdate(updateId, &buffer);
}

bool QnServerUpdateTool::addUpdateFileChunk(const QString &updateId, const QByteArray &data, qint64 offset) {
    if (m_bannedUpdates.contains(updateId))
        return false;

    if (m_updateId != updateId) {
        m_chunks.clear();
        m_file.reset();
        clearUpdatesLocation();
        m_updateId = updateId;
    }

    if (!m_file) {
        m_file.reset(new QFile(getUpdateFilePath(updateId)));
        if (!m_file->open(QFile::WriteOnly)) {
            cl_log.log("Could not save update to ", m_file->fileName(), cl_logERROR);
            m_bannedUpdates.insert(updateId);
            return false;
        }
    }

    if (data.isEmpty()) { // it means we're just got the size of the file
        m_length = offset;
    } else {
        m_file->seek(offset);
        m_file->write(data);
        m_chunks.insert(offset, data.size());
        sendReply();
    }

    if (isComplete()) {
        m_file->close();

        bool ok = processUpdate(updateId, m_file.data());

        m_file.reset();

        sendReply(ok ? ec2::AbstractUpdatesManager::Finished : ec2::AbstractUpdatesManager::Failed);

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

    QStringList arguments;

    QString logFileName;
    if (initializeUpdateLog(updateId, &logFileName))
        arguments.append(logFileName);
    else
        cl_log.log("Could not create or open update log file.", cl_logWARNING);

    if (QProcess::startDetached(updateDir.absoluteFilePath(executable), arguments)) {
        cl_log.log("Update has been started.", cl_logINFO);
    } else {
        cl_log.log("Update failed. See update log for details: ", logFileName, cl_logERROR);
    }

    QDir::setCurrent(currentDir);

    return true;
}

void QnServerUpdateTool::addChunk(qint64 offset, int length) {
    m_chunks[offset] = length;
}

bool QnServerUpdateTool::isComplete() const {
    if (m_length == -1)
        return false;

    if (m_chunks.size() == 1)
        return m_chunks.firstKey() == 0 && m_chunks.first() == m_length;

    int chunkCount = (m_length + m_chunks.first() - 1) / m_chunks.first();
    return m_chunks.size() == chunkCount;
}

void QnServerUpdateTool::clearUpdatesLocation() {
    getUpdatesDir().removeRecursively();
}
