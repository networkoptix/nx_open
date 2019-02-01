#include "server_update_tool.h"

#include <chrono>

#include <QtCore/QDir>
#include <QtCore/QBuffer>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>
#include <QtCore/QElapsedTimer>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <common/static_common_module.h>
#include <media_server/media_server_module.h>
#include <media_server/serverutil.h>
#include <media_server/settings.h>
#include <nx_ec/ec_api.h>
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>
#include <utils/common/process.h>
#include <utils/update/update_utils.h>
#include <utils/update/zip_utils.h>

#include <nx/vms/server/root_fs.h>
#include <nx/utils/log/log.h>

namespace {

using namespace std::chrono;
using namespace std::literals::chrono_literals;

static const QString kUpdatesDirSuffix = lit("mediaserver/updates");
static const QString kUpdateInfoFileName = lit("update.json");
static const QString kUpdateLogFileName = lit("update.log");
constexpr std::chrono::milliseconds kInstallationDelay = 15s;

} // namespace

QnServerUpdateTool::QnServerUpdateTool(QnMediaServerModule* serverModule)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_mutex(QnMutex::Recursive)
{}

QnServerUpdateTool::~QnServerUpdateTool()
{
}

QDir QnServerUpdateTool::getUpdatesDir() const
{
    const QString& dataDir = settings().dataDir();
    QDir dir = dataDir.isEmpty() ? QDir::temp() : dataDir;
    if (!dir.exists(kUpdatesDirSuffix))
        dir.mkpath(kUpdatesDirSuffix);
    dir.cd(kUpdatesDirSuffix);
    return dir;
}

QDir QnServerUpdateTool::getUpdateDir(const QString& updateId) const
{
    QUuid uuid(updateId);
    QString id = uuid.isNull() ? updateId : updateId.mid(1, updateId.length() - 2);
    QDir dir = getUpdatesDir();
    if (!dir.exists(id))
        dir.mkdir(id);
    dir.cd(id);
    return dir;
}

QString QnServerUpdateTool::getUpdateFilePath(const QString& updateId) const
{
    return getUpdatesDir().absoluteFilePath(updateId + lit(".zip"));
}

bool QnServerUpdateTool::initializeUpdateLog(const QString& targetVersion, QString* logFileName) const
{
    QString logDir;
    if (settings().logDir.present())
        logDir = settings().logDir();
    else
        logDir = settings().dataDir() + lit("/log/");

    if (logDir.isEmpty())
        return false;

    QString fileName = QDir(logDir).absoluteFilePath(kUpdateLogFileName);
    QFile logFile(fileName);
    if (!logFile.open(QFile::Append))
        return false;

    QByteArray preface;
    preface.append("================================================================================\n");
    preface.append(QString(lit(" [%1] Starting system update:\n")).arg(QDateTime::currentDateTime().toString()));
    preface.append(QString(lit("    Current version: %1\n")).arg(qnStaticCommon->engineVersion().toString()));
    preface.append(QString(lit("    Target version: %1\n")).arg(targetVersion));
    preface.append("================================================================================\n");

    logFile.write(preface);
    logFile.close();

    *logFileName = fileName;
    return true;
}

QnServerUpdateTool::ReplyCode QnServerUpdateTool::processUpdate(const QString& updateId, QIODevice* ioDevice, bool sync) {
    if (!m_fileMd5.isEmpty() && makeMd5(ioDevice) != m_fileMd5) {
        NX_WARNING(this, lit("Checksum test failed: %1").arg(getUpdateFilePath(updateId)));
        return UnknownError;
    }

    m_zipExtractor.reset(new QnZipExtractor(ioDevice, getUpdateDir(updateId).absolutePath()));

    if (sync) {
        m_zipExtractor->extractZip();
        bool ok = m_zipExtractor->error() == QnZipExtractor::Ok;
        if (ok) {
            NX_INFO(this, lit("Update package has been extracted to %1").arg(getUpdateDir(updateId).path()));
        } else {
            NX_WARNING(this, lit("Could not extract update package. Error message: %1")
                   .arg(QnZipExtractor::errorToString(static_cast<QnZipExtractor::Error>(m_zipExtractor->error()))));
        }
        m_zipExtractor.reset();
        if (ok)
            return UploadFinished;
        else
            return m_zipExtractor->error() == QnZipExtractor::NoFreeSpace ? NoFreeSpace : UnknownError;
    } else {
        connect(m_zipExtractor.data(), &QnZipExtractor::finished, this, &QnServerUpdateTool::at_zipExtractor_extractionFinished);
        m_zipExtractor->start();
    }
    return NoReply;
}

void QnServerUpdateTool::sendReply(int code)
{
    NX_VERBOSE(this, lit("Update chunk reply [id = %1, code = %2].").arg(m_updateId).arg(code));
    ec2Connection()->getUpdatesManager(Qn::kSystemAccess)->sendUpdateUploadResponce(
                m_updateId, moduleGUID(), code, this, [this](int, ec2::ErrorCode) {});
}

bool QnServerUpdateTool::addUpdateFile(const QString& updateId, const QByteArray& data) {
    NX_VERBOSE(this, lit("Update file added [size = %1].").arg(data.size()));
    m_zipExtractor.reset();
    clearUpdatesLocation();
    QBuffer buffer(const_cast<QByteArray*>(&data)); // we're goint to read data, so const_cast is ok here
    buffer.open(QBuffer::ReadOnly);
    return processUpdate(updateId, &buffer, true);
}

qint64 QnServerUpdateTool::addUpdateFileChunkSync(const QString& updateId, const QByteArray& data, qint64 offset)
{
    qint64 reply = addUpdateFileChunkInternal(updateId, data, offset);

    switch (reply) {
    case UploadFinished:
        return processUpdate(updateId, m_file.data(), true);
    case NoFreeSpace:
        return NoFreeSpace;
    case UnknownError:
    case NoReply:
        return UnknownError;
    default:
        NX_ASSERT(reply >= 0, "wrong reply code");
        if (reply >= 0)
            return reply;
        return UnknownError;
    }
}

void QnServerUpdateTool::addUpdateFileChunkAsync(const QString& updateId, const QByteArray& data, qint64 offset)
{
    qint64 reply = addUpdateFileChunkInternal(updateId, data, offset);

    switch (reply) {
    case UploadFinished:
        if (processUpdate(updateId, m_file.data(), false) != NoReply) {
            m_file->remove();
            sendReply(ec2::AbstractUpdatesManager::UnknownError);
            return;
        }
        break;
    case NoReply:
        return;
    case NoFreeSpace:
        sendReply(NoFreeSpace);
        return;
    case UnknownError:
        sendReply(ec2::AbstractUpdatesManager::UnknownError);
        break;
    default:
        NX_ASSERT(reply >= 0, "wrong reply code");
        if (reply >= 0) {
            /* we work with files < 500 MB, so int type is ok */
            sendReply(static_cast<int>(reply));
        }
        break;
    }
}

qint64 QnServerUpdateTool::addUpdateFileChunkInternal(const QString& updateId,
    const QByteArray& data, qint64 offset)
{
    NX_VERBOSE(
        this,
        lm("Update chunk [id = %1, offset = %2, size = %3].")
            .arg(updateId).arg(offset).arg(data.size()));


    if (m_bannedUpdates.contains(updateId))
    {
        NX_ERROR(this, lm("Update %1 is banned.").arg(updateId));
        return NoReply;
    }

    if (m_updateId != updateId) {
        m_file.reset();
        m_zipExtractor.reset();
        clearUpdatesLocation(updateId);
        m_updateId = updateId;
    }

    if (offset < 0) {
        m_fileMd5 = data;
        m_file.reset();
    }

    if (!m_file) {
        m_file.reset(new QFile(getUpdateFilePath(updateId)));
        if (!m_file->open(QFile::ReadWrite)) {
            NX_ERROR(this, lm("Could not save update to %1").arg(m_file->fileName()));
            m_bannedUpdates.insert(updateId);
            return UnknownError;
        }
        m_file->seek(m_file->size());
        return m_file->pos();
    }

    /* Closed file means we've already finished downloading. Nothing to do is left. */
    if (!m_file->isOpen() || !m_file->openMode().testFlag(QFile::WriteOnly))
        return NoReply;

    if (!data.isEmpty())
    {
        if (m_file->pos() < offset)
        {
            NX_ERROR(this,
                lm("The requested offset %1 is more than current position %2.")
                    .arg(offset).arg(m_file->pos()));

            return UnknownError;
        }

        m_file->seek(offset);

        if (m_file->write(data) != data.size())
        {
            NX_ERROR(this, lm("Cannot write to %1").arg(m_file->fileName()));
            return NoFreeSpace;
        }

        const auto pos = m_file->pos();

        if (m_file->size() > pos)
        {
            NX_WARNING(this,
                lm("Update file size %1 is more than current position %2. Truncating...")
                    .arg(m_file->size()).arg(pos));

            m_file->resize(pos);
        }

        return m_file->pos();
    }

    /* it means we've just got the size of the file */
    if (m_file->pos() != offset)
        return m_file->pos();

    m_file->close();
    m_file->open(QFile::ReadOnly);

    return UploadFinished;
}

bool QnServerUpdateTool::installUpdate(const QString& updateId, UpdateType updateType)
{
    if (updateType == UpdateType::Delayed)
    {
        NX_INFO(this, lm("Requested delayed installation of %1. Installing in %2 ms.")
                .args(updateId, kInstallationDelay.count()));

        executeDelayed(
            [updateId, this]() { installUpdate(updateId); },
            kInstallationDelay.count(),
            thread());

        return true;
    }

    NX_INFO(this, lit("Starting update to %1").arg(updateId));

    QDir updateDir = getUpdateDir(updateId);
    if (!updateDir.exists()) {
        NX_ERROR(this, lit("Update dir does not exist: %1").arg(updateDir.path()));
        return false;
    }

    QFile updateInfoFile(updateDir.absoluteFilePath(kUpdateInfoFileName));
    if (!updateInfoFile.open(QFile::ReadOnly)) {
        NX_ERROR(this, lit("Could not open update information file: %1").arg(updateInfoFile.fileName()));
        return false;
    }

    QVariantMap map = QJsonDocument::fromJson(updateInfoFile.readAll()).toVariant().toMap();
    updateInfoFile.close();

    QString executable = map.value(lit("executable")).toString();
    if (executable.isEmpty()) {
        NX_ERROR(this, lit("There is no executable specified in update information file: ").arg(updateInfoFile.fileName()));
        return false;
    }

    const auto systemInformation = QnAppInfo::currentSystemInformation();

    QString platform = map.value(lit("platform")).toString();
    if (platform != systemInformation.platform) {
        NX_ERROR(this, lit("Incompatible update: %1 != %2").arg(systemInformation.platform).arg(platform));
        return false;
    }

    QString arch = map.value(lit("arch")).toString();
    if (arch != systemInformation.arch) {
        NX_ERROR(this, lit("Incompatible update: %1 != %2").arg(systemInformation.arch).arg(arch));
        return false;
    }

    QString modification = map.value(lit("modification")).toString();
    if (modification != systemInformation.modification) {
        NX_ERROR(this, lit("Incompatible update: %1 != %2").arg(systemInformation.modification).arg(modification));
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
        NX_WARNING(this, "Could not create or open update log file.");

    QFile executableFile(updateDir.absoluteFilePath(executable));
    if (!executableFile.exists()) {
        NX_ERROR(this, lit("The specified executable doesn't exists: %1").arg(executable));
        return false;
    }
    if (!executableFile.permissions().testFlag(QFile::ExeOwner)) {
        NX_WARNING(this, lit("The specified executable doesn't have an execute permission: %1").arg(executable));
        executableFile.setPermissions(executableFile.permissions() | QFile::ExeOwner);
    }
    if (nx::utils::log::mainLogger()->isToBeLogged(nx::utils::log::Level::debug))
    {
        QString argumentsStr(" APPSERVER_PASSWORD=\"\" APPSERVER_PASSWORD_CONFIRM=\"\" SERVER_PASSWORD=\"\" SERVER_PASSWORD_CONFIRM=\"\"");
        for( const QString& arg: arguments )
            argumentsStr += lit(" ") + arg;

        NX_INFO(this, lit("Launching %1 %2").arg(executable).arg(argumentsStr));
    }

    const SystemError::ErrorCode processStartErrorCode = nx::startProcessDetached( updateDir.absoluteFilePath(executable), arguments );
    if( processStartErrorCode == SystemError::noError ) {
        NX_INFO(this, "Update has been started.");
    } else {
        NX_ERROR(this, lit("Cannot launch update script. %1").arg(SystemError::toString(processStartErrorCode)));
    }

    QDir::setCurrent(currentDir);

    return true;
}

void QnServerUpdateTool::removeUpdateFiles(const QString& updateId)
{
    auto dir = getUpdatesDir();
    if (dir.cd(updateId))
        dir.removeRecursively();
}

void QnServerUpdateTool::clearUpdatesLocation(const QString& idToLeave)
{
    QDir dir = getUpdatesDir();

    serverModule()->rootFileSystem()->changeOwner(dir.absolutePath());

    QString fileToLeave = idToLeave + ".zip";

    for (const QFileInfo &info: dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (info.fileName() == fileToLeave)
            continue;

        if (info.isDir())
            QDir(info.absoluteFilePath()).removeRecursively();
        else
            dir.remove(info.fileName());
    }
}

void QnServerUpdateTool::at_zipExtractor_extractionFinished(int error)
{
    if (sender() != m_zipExtractor.data())
        return;

    m_file->close();

    ec2::AbstractUpdatesManager::ReplyCode code = ec2::AbstractUpdatesManager::NoError;

    if (error == QnZipExtractor::Ok) {
        NX_INFO(this, lit("Update package has been extracted to %1").arg(m_zipExtractor->dir().absolutePath()));
    } else {
        if (error == QnZipExtractor::NoFreeSpace)
            code = ec2::AbstractUpdatesManager::NoFreeSpace;
        else
            code = ec2::AbstractUpdatesManager::UnknownError;

        NX_WARNING(this, lit("Could not extract update package. Error message: %1").arg(QnZipExtractor::errorToString(static_cast<QnZipExtractor::Error>(error))));
    }

    m_zipExtractor.reset();
    sendReply(code);
}
