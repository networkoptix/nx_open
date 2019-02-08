#include "applauncher_api.h"

#include <QtCore/QList>

namespace {

const auto kArgumentsDelimiter = lit("@#$%^delim");

} // namespace

namespace applauncher {
namespace api {

namespace TaskType {

Value fromString(const QnByteArrayConstRef& str)
{
    if (str == "run")
        return startApplication;
    if (str == "quit")
        return quit;
    if (str == "installZip")
        return installZip;
    if (str == "isVersionInstalled")
        return isVersionInstalled;
    if (str == "getInstalledVersions")
        return getInstalledVersions;
    if (str == "addProcessKillTimer")
        return addProcessKillTimer;
    return invalidTaskType;
}

QByteArray toString(Value val)
{
    switch (val)
    {
        case startApplication:
            return "run";
        case quit:
            return "quit";
        case installZip:
            return "installZip";
        case isVersionInstalled:
            return "isVersionInstalled";
        case getInstalledVersions:
            return "getInstalledVersions";
        case addProcessKillTimer:
            return "addProcessKillTimer";
        default:
            return "unknown";
    }
}

} // namespace TaskType

QString launcherPipeName()
{
    const auto baseName = nx::utils::AppInfo::customizationName()
        + lit("EC4C367A-FEF0-4fa9-B33D-DF5B0C767788");

    return nx::utils::AppInfo::isMacOsX()
        ? baseName + QString::fromLatin1(qgetenv("USER").toBase64())
        : baseName;
}

BaseTask::BaseTask(TaskType::Value _type)
    :
    type(_type)
{
}

bool deserializeTask(const QByteArray& str, BaseTask** ptr)
{
    const int taskNameEnd = str.indexOf('\n');
    if (taskNameEnd == -1)
        return false;
    const TaskType::Value taskType = TaskType::fromString(
        QnByteArrayConstRef(str, 0, taskNameEnd));
    switch (taskType)
    {
        case TaskType::startApplication:
            *ptr = new StartApplicationTask();
            break;

        case TaskType::quit:
            *ptr = new QuitTask();
            break;

        case TaskType::installZip:
            *ptr = new InstallZipTask();
            break;

        case TaskType::isVersionInstalled:
            *ptr = new IsVersionInstalledRequest();
            break;

        case TaskType::getInstalledVersions:
            *ptr = new GetInstalledVersionsRequest();
            break;

        case TaskType::addProcessKillTimer:
            *ptr = new AddProcessKillTimerRequest();
            break;

        case TaskType::invalidTaskType:
            return false;
    }

    if (!(*ptr)->deserialize(str))
    {
        delete *ptr;
        *ptr = NULL;
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////
//// class StartApplicationTask
////////////////////////////////////////////////////////////
StartApplicationTask::StartApplicationTask()
    :
    BaseTask(TaskType::startApplication)
{
}

StartApplicationTask::StartApplicationTask(
    const nx::utils::SoftwareVersion& _version,
    const QStringList& _appParams)
    :
    BaseTask(TaskType::startApplication),
    version(_version),
    appArgs( _appParams )
{
}

QByteArray StartApplicationTask::serialize() const
{
    const auto res = lit("%1\n%2\n%3\n\n")
        .arg(QLatin1String(TaskType::toString(type)))
        .arg(version.toString())
        .arg(appArgs.join(kArgumentsDelimiter)).toLatin1();
    return res;
}

bool StartApplicationTask::deserialize(const QnByteArrayConstRef& data)
{
    const QList<QnByteArrayConstRef>& lines = data.split('\n');
    if (lines.size() < 3)
        return false;
    if (lines[0] != TaskType::toString(type))
        return false;
    version = nx::utils::SoftwareVersion(lines[1].toByteArrayWithRawData());
    const auto stringArguments = QString::fromLatin1(lines[2].toByteArrayWithRawData());
    appArgs = stringArguments.split(kArgumentsDelimiter, QString::SkipEmptyParts);

    return true;
}

////////////////////////////////////////////////////////////
//// class QuitTask
////////////////////////////////////////////////////////////
QuitTask::QuitTask():
    BaseTask(TaskType::quit)
{
}

QByteArray QuitTask::serialize() const
{
    return lit("%1\n\n").arg(QLatin1String(TaskType::toString(type))).toLatin1();
}

bool QuitTask::deserialize(const QnByteArrayConstRef& data)
{
    const QList<QnByteArrayConstRef>& lines = data.split('\n');
    if (lines.empty())
        return false;
    if (lines[0] != TaskType::toString(type))
        return false;
    return true;
}

////////////////////////////////////////////////////////////
//// class Response
////////////////////////////////////////////////////////////
IsVersionInstalledRequest::IsVersionInstalledRequest()
    :
    BaseTask(TaskType::isVersionInstalled)
{
}

QByteArray IsVersionInstalledRequest::serialize() const
{
    return lit("%1\n%2\n\n").arg(QLatin1String(TaskType::toString(type))).arg(version.toString()).
        toLatin1();
}

bool IsVersionInstalledRequest::deserialize(const QnByteArrayConstRef& data)
{
    const QList<QnByteArrayConstRef>& lines = data.split('\n');
    if (lines.size() < 2)
        return false;
    if (lines[0] != TaskType::toString(type))
        return false;
    version = nx::utils::SoftwareVersion(lines[1].toByteArrayWithRawData());
    return true;
}

////////////////////////////////////////////////////////////
//// class Response
////////////////////////////////////////////////////////////
namespace ResultType {
Value fromString(const QnByteArrayConstRef& str)
{
    if (str == "ok")
        return ok;
    if (str == "connectError")
        return connectError;
    if (str == "versionNotInstalled")
        return versionNotInstalled;
    if (str == "alreadyInstalled")
        return alreadyInstalled;
    if (str == "invalidVersionFormat")
        return invalidVersionFormat;
    if (str == "notFound")
        return notFound;
    if (str == "ioError")
        return ioError;
    if (str == "notEnoughSpace")
        return notEnoughSpace;
    if (str == "brokenPackage")
        return brokenPackage;
    return otherError;
}

QByteArray toString(Value val)
{
    switch (val)
    {
        case ok:
            return "ok";
        case connectError:
            return "connectError";
        case versionNotInstalled:
            return "versionNotInstalled";
        case alreadyInstalled:
            return "alreadyInstalled";
        case invalidVersionFormat:
            return "invalidVersionFormat";
        case notFound:
            return "notFound";
        case ioError:
            return "ioError";
        case notEnoughSpace:
            return "notEnoughSpace";
        case brokenPackage:
            return "brokenPackage";
        default:
            return "otherError " + QByteArray::number(val);
    }
}
}

QByteArray Response::serialize() const
{
    return ResultType::toString(result) + "\n";
}

bool Response::deserialize(const QnByteArrayConstRef& data)
{
    int sepPos = data.indexOf('\n');
    if (sepPos == -1)
        return false;
    result = ResultType::fromString(data.mid(0, sepPos));
    return true;
}

////////////////////////////////////////////////////////////
//// class InstallationDirRequest
////////////////////////////////////////////////////////////
InstallZipTask::InstallZipTask()
    :
    BaseTask(TaskType::installZip)
{
}

InstallZipTask::InstallZipTask(
    const nx::utils::SoftwareVersion& version,
    const QString& zipFileName)
    :
    BaseTask(TaskType::installZip),
    version(version),
    zipFileName(zipFileName)
{
}

QByteArray InstallZipTask::serialize() const
{
    return lit("%1\n%2\n%3\n\n")
        .arg(QString::fromLatin1(TaskType::toString(type)))
        .arg(version.toString())
        .arg(zipFileName).toUtf8();
}

bool InstallZipTask::deserialize(const QnByteArrayConstRef& data)
{
    QStringList list = QString::fromUtf8(data).split(lit("\n"), QString::SkipEmptyParts);
    if (list.size() < 3)
        return false;
    if (TaskType::toString(type) != list[0].toLatin1())
        return false;
    version = nx::utils::SoftwareVersion(list[1]);
    zipFileName = list[2];
    return true;
}

////////////////////////////////////////////////////////////
//// class IsVersionInstalledResponse
////////////////////////////////////////////////////////////
QByteArray IsVersionInstalledResponse::serialize() const
{
    return Response::serialize() + QByteArray::number(installed ? 1 : 0) + "\n\n";
}

bool IsVersionInstalledResponse::deserialize(const QnByteArrayConstRef& data)
{
    if (!Response::deserialize(data))
        return false;

    const QList<QnByteArrayConstRef>& lines = data.split('\n');
    if (lines.size() < 2)
        return false;
    //line 0 - is a error code
    installed = lines[1].toUInt() > 0;
    return true;
}

////////////////////////////////////////////////////////////
//// class AddProcessKillTimerRequest
////////////////////////////////////////////////////////////
AddProcessKillTimerRequest::AddProcessKillTimerRequest():
    BaseTask(TaskType::addProcessKillTimer)
{
}

QByteArray AddProcessKillTimerRequest::serialize() const
{
    return lit("%1\n%2\n%3\n\n").arg(QLatin1String(TaskType::toString(type))).arg(processID).arg(
        timeoutMillis).toLatin1();
}

bool AddProcessKillTimerRequest::deserialize(const QnByteArrayConstRef& data)
{
    const QList<QnByteArrayConstRef>& lines = data.split('\n');
    if (lines.size() < 3)
        return false;
    if (lines[0] != TaskType::toString(type))
        return false;
    processID = lines[1].toByteArrayWithRawData().toUInt();
    timeoutMillis = lines[2].toByteArrayWithRawData().toUInt();
    return true;
}

////////////////////////////////////////////////////////////
//// class GetInstalledVersionsRequest
////////////////////////////////////////////////////////////
GetInstalledVersionsRequest::GetInstalledVersionsRequest()
    :
    BaseTask(TaskType::getInstalledVersions)
{
}

QByteArray GetInstalledVersionsRequest::serialize() const
{
    return TaskType::toString(type) + "\n";
}

bool GetInstalledVersionsRequest::deserialize(const QnByteArrayConstRef& data)
{
    return data == serialize();
}

////////////////////////////////////////////////////////////
//// class GetInstalledVersionsResponse
////////////////////////////////////////////////////////////

QByteArray GetInstalledVersionsResponse::serialize() const
{
    QByteArray result = Response::serialize();
    for (const auto& version: versions)
    {
        result.append(version.toString().toLatin1());
        result.append(',');
    }
    result[result.size() - 1] = '\n';
    return result;
}

bool GetInstalledVersionsResponse::deserialize(const QnByteArrayConstRef& data)
{
    if (!Response::deserialize(data))
        return false;

    const QList<QnByteArrayConstRef>& lines = data.split('\n');
    if (lines.size() < 2)
        return false;

    const QList<QnByteArrayConstRef>& versions = lines[1].split(',');
    for (const QnByteArrayConstRef& version: versions)
        this->versions.append(nx::utils::SoftwareVersion(version.toByteArrayWithRawData()));

    return true;
}

} // namespace api
} // namespace applauncher
