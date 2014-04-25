#include "server_update_tool.h"

#include <QtCore/QDir>
#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <media_server/settings.h>
#include <utils/common/log.h>

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
