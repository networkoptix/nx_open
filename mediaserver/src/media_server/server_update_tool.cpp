#include "server_update_tool.h"

#include <QtCore/QDir>
#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <media_server/settings.h>
#include <media_server/serverutil.h>
#include <utils/common/log.h>
#include <utils/update/update_utils.h>
#include <utils/update/zip_utils.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <media_server/serverutil.h>

namespace {

    const QString updatesDirSuffix = lit("mediaserver/updates");
    const QString updateInfoFileName = lit("update.json");
    const QString updateLogFileName = lit("update.log");
    const int replyDelay = 200;

    QDir getUpdatesDir() {
        const QString& dataDir = MSSettings::roSettings()->value( "dataDir" ).toString();
        QDir dir = dataDir.isEmpty() ? QDir::temp() : dataDir;
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
        preface.append(QString(lit("    Current version: %1\n")).arg(qnCommon->engineVersion().toString()));
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

QnServerUpdateTool::QnServerUpdateTool() :
    m_mutex(QMutex::Recursive),
    m_length(-1)
{}

QnServerUpdateTool::~QnServerUpdateTool() {}

bool QnServerUpdateTool::processUpdate(const QString &updateId, QIODevice *ioDevice, bool sync) {
    m_zipExtractor.reset(new QnZipExtractor(ioDevice, getUpdateDir(updateId).absolutePath()));
    m_zipExtractor->start();

    if (sync) {
        m_zipExtractor->wait();
        bool ok = m_zipExtractor->error() == QnZipExtractor::Ok;
        if (ok) {
            NX_LOG(lit("Update package has been extracted to %1").arg(getUpdateDir(m_updateId).path()), cl_logINFO);
        } else {
            NX_LOG(lit("Could not extract update package. Error message: %1").arg(QnZipExtractor::errorToString(static_cast<QnZipExtractor::Error>(m_zipExtractor->error()))), cl_logWARNING);
        }
        m_zipExtractor.reset();
        return ok;
    } else {
        connect(m_zipExtractor.data(), &QnZipExtractor::finished, this, &QnServerUpdateTool::at_zipExtractor_extractionFinished);
    }
    return true;
}

void QnServerUpdateTool::sendReply(int code) {
    connection2()->getUpdatesManager()->sendUpdateUploadResponce(m_updateId, qnCommon->moduleGUID(), code == 0 ? m_chunks.size() : code,
                                                                 this, [this](int, ec2::ErrorCode) {});
}

bool QnServerUpdateTool::addUpdateFile(const QString &updateId, const QByteArray &data) {
    if (m_zipExtractor)
        m_zipExtractor->stop();

    clearUpdatesLocation();
    QBuffer buffer(const_cast<QByteArray*>(&data)); // we're goint to read data, so const_cast is ok here
    return processUpdate(updateId, &buffer, true);
}

void QnServerUpdateTool::addUpdateFileChunk(const QString &updateId, const QByteArray &data, qint64 offset) {
    if (m_bannedUpdates.contains(updateId))
        return;

    if (m_updateId != updateId) {
        m_chunks.clear();
        m_file.reset();
        if (m_zipExtractor) {
            m_zipExtractor->stop();
            m_zipExtractor.reset();
        }
        clearUpdatesLocation();
        m_updateId = updateId;
    }

    if (!m_file) {
        m_file.reset(new QFile(getUpdateFilePath(updateId)));
        if (!m_file->open(QFile::WriteOnly)) {
            NX_LOG(lit("Could not save update to %1").arg(m_file->fileName()), cl_logERROR);
            m_bannedUpdates.insert(updateId);
            return;
        }
    }

    // Closed file means we've already finished downloading. Nothing to do is left.
    if (!m_file->isOpen())
        return;

    if (data.isEmpty()) { // it means we've just got the size of the file
        m_length = offset;
    } else {
        m_file->seek(offset);
        m_file->write(data);
        m_chunks.insert(offset, data.size());
        sendReply();
    }

    if (isComplete()) {
        m_file->close();
        processUpdate(updateId, m_file.data());
    }
}

bool QnServerUpdateTool::installUpdate(const QString &updateId) {
    NX_LOG(lit("Starting update to %1").arg(updateId), cl_logINFO);

    QDir updateDir = getUpdateDir(updateId);
    if (!updateDir.exists()) {
        NX_LOG(lit("Update dir does not exist: %1").arg(updateDir.path()), cl_logERROR);
        return false;
    }

    QFile updateInfoFile(updateDir.absoluteFilePath(updateInfoFileName));
    if (!updateInfoFile.open(QFile::ReadOnly)) {
        NX_LOG(lit("Could not open update information file: %1").arg(updateInfoFile.fileName()), cl_logERROR);
        return false;
    }

    QVariantMap map = QJsonDocument::fromJson(updateInfoFile.readAll()).toVariant().toMap();
    updateInfoFile.close();

    QString executable = map.value(lit("executable")).toString();
    if (executable.isEmpty()) {
        NX_LOG(lit("There is no executable specified in update information file: ").arg(updateInfoFile.fileName()), cl_logERROR);
        return false;
    }

    QnSystemInformation systemInformation = QnSystemInformation::currentSystemInformation();

    QString platform = map.value(lit("platform")).toString();
    if (platform != systemInformation.platform) {
        NX_LOG(lit("Incompatible update: %1 != %2").arg(systemInformation.platform).arg(platform), cl_logERROR);
        return false;
    }

    QString arch = map.value(lit("arch")).toString();
    if (arch != systemInformation.arch) {
        NX_LOG(lit("Incompatible update: %1 != %2").arg(systemInformation.arch).arg(arch), cl_logERROR);
        return false;
    }

    QString modification = map.value(lit("modification")).toString();
    if (modification != systemInformation.modification) {
        NX_LOG(lit("Incompatible update: %1 != %2").arg(systemInformation.modification).arg(modification), cl_logERROR);
        return false;
    }

    if (!backupDatabase()) {
        NX_LOG("Could not create database backup.", cl_logERROR);
        return false;
    }

    QString version = map.value(lit("version")).toString();

    QString currentDir = QDir::currentPath();
    QDir::setCurrent(updateDir.absolutePath());

    QStringList arguments;

    QString logFileName;
    if (initializeUpdateLog(version, &logFileName))
        arguments.append(logFileName);
    else
        NX_LOG("Could not create or open update log file.", cl_logWARNING);

    QFile executableFile(updateDir.absoluteFilePath(executable));
    if (!executableFile.exists()) {
        NX_LOG(lit("The specified executable doesn't exists: %1").arg(executable), cl_logERROR);
        return false;
    }
    if (!executableFile.permissions().testFlag(QFile::ExeOwner)) {
        NX_LOG(lit("The specified executable doesn't have an execute permission: %1").arg(executable), cl_logWARNING);
        executableFile.setPermissions(executableFile.permissions() | QFile::ExeOwner);
    }

    if (QProcess::startDetached(updateDir.absoluteFilePath(executable), arguments)) {
        NX_LOG("Update has been started.", cl_logINFO);
    } else {
        NX_LOG( lit("Update failed. See update log for details: %1").arg(logFileName), cl_logERROR);
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

void QnServerUpdateTool::at_zipExtractor_extractionFinished(int error) {
    QnZipExtractor *zipExtractor = qobject_cast<QnZipExtractor*>(sender());
    if (!zipExtractor)
        return;

    zipExtractor->deleteLater();

    bool ok = (error == QnZipExtractor::Ok);

    if (ok) {
        NX_LOG(lit("Update package has been extracted to %1").arg(zipExtractor->dir().absolutePath()), cl_logINFO);
    } else {
        NX_LOG(lit("Could not extract update package. Error message: %1").arg(QnZipExtractor::errorToString(static_cast<QnZipExtractor::Error>(error))), cl_logWARNING);
    }

    sendReply(ok ? ec2::AbstractUpdatesManager::Finished : ec2::AbstractUpdatesManager::Failed);
}
