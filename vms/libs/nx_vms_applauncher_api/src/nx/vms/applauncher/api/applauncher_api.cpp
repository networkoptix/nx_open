#include "applauncher_api.h"

#include <optional>

#include <QtCore/QHash>

#include <nx/utils/app_info.h>
#include <nx/utils/log/log.h>

namespace nx::vms::applauncher::api {

namespace {

const auto kArgumentsDelimiter = "@#$%^delim";

QByteArray serializeParameters(const QByteArray& type, const QStringList& parameters)
{
    QByteArray result = type + '\n';

    for (const QString& param: parameters)
    {
        result += param.toUtf8();
        result += '\n';
    }
    result += '\n';

    return result;
}

std::optional<QStringList> deserializeParameters(
    const QByteArray& type, int parametersCount, const QByteArray& data, bool verifyType)
{
    QList<QByteArray> lines = data.split('\n');
    if (lines.size() < parametersCount + 1)
        return {};
    if (verifyType && lines.takeFirst() != type)
        return {};

    QStringList result;
    for (const QByteArray& line: lines)
        result.append(QString::fromUtf8(line));
    return std::make_optional(result);
}


QByteArray serializeTaskParameters(TaskType type, const QStringList& parameters)
{
    return serializeParameters(serializeTaskType(type), parameters);
}

std::optional<QStringList> deserializeTaskParameters(
    TaskType type, int parametersCount, const QByteArray& data)
{
    return deserializeParameters(serializeTaskType(type), parametersCount, data, true);
}

QByteArray serializeResponseParameters(ResultType type, const QStringList& parameters)
{
    return serializeParameters(serializeResultType(type), parameters);
}

std::optional<QStringList> deserializeResponseParameters(
    ResultType* type, int parametersCount, const QByteArray& data)
{
    auto result = deserializeParameters(QByteArray(), parametersCount, data, false);
    if (result)
    {
        const QString typeString = result->takeFirst();
        if (type)
            *type = deserializeResultType(typeString.toUtf8());
    }
    return result;
}

} // namespace

QByteArray serializeTaskType(TaskType value)
{
    switch (value)
    {
        case TaskType::startApplication:
            return "run";
        case TaskType::quit:
            return "quit";
        case TaskType::installZip:
            return "installZip";
        case TaskType::startZipInstallation:
            return "startZipInstallation";
        case TaskType::checkZipProgress:
            return "checkZipProgress";
        case TaskType::isVersionInstalled:
            return "isVersionInstalled";
        case TaskType::getInstalledVersions:
            return "getInstalledVersions";
        case TaskType::addProcessKillTimer:
            return "addProcessKillTimer";
        case TaskType::pingApplauncher:
            return "pingApplauncher";
        default:
            return "unknown";
    }
}

QString toString(TaskType value)
{
    return QString::fromLatin1(serializeTaskType(value));
}

ResultType deserializeResultType(const QByteArray& string)
{
    static QHash<QByteArray, ResultType> kStringToTaskType{
        {"ok", ResultType::ok},
        {"connectError", ResultType::connectError},
        {"versionNotInstalled", ResultType::versionNotInstalled},
        {"alreadyInstalled", ResultType::alreadyInstalled},
        {"invalidVersionFormat", ResultType::invalidVersionFormat},
        {"notFound", ResultType::notFound},
        {"ioError", ResultType::ioError},
        {"notEnoughSpace", ResultType::notEnoughSpace},
        {"brokenPackage", ResultType::brokenPackage},
        {"unpackingZip", ResultType::unpackingZip},
        {"busy", ResultType::busy}
    };
    return kStringToTaskType.value(string, ResultType::otherError);
}

QByteArray serializeResultType(ResultType value)
{
    switch (value)
    {
        case ResultType::ok:
            return "ok";
        case ResultType::connectError:
            return "connectError";
        case ResultType::versionNotInstalled:
            return "versionNotInstalled";
        case ResultType::alreadyInstalled:
            return "alreadyInstalled";
        case ResultType::invalidVersionFormat:
            return "invalidVersionFormat";
        case ResultType::notFound:
            return "notFound";
        case ResultType::ioError:
            return "ioError";
        case ResultType::notEnoughSpace:
            return "notEnoughSpace";
        case ResultType::brokenPackage:
            return "brokenPackage";
        case ResultType::unpackingZip:
            return "unpackingZip";
        case ResultType::busy:
            return "busy";
        default:
            return "otherError " + QByteArray::number((int) value);
    }
}

QString toString(ResultType value)
{
    return QString::fromLatin1(serializeResultType(value));
}

QString launcherPipeName()
{
    QString baseName = nx::utils::AppInfo::customizationName()
        + "EC4C367A-FEF0-4fa9-B33D-DF5B0C767788";

    if (nx::utils::AppInfo::isMacOsX())
        baseName += QString::fromLatin1(qgetenv("USER").toBase64());

    return baseName;
}

QByteArray BaseTask::serialize() const
{
    return serializeTaskParameters(type, {});
}

bool BaseTask::deserialize(const QByteArray& data)
{
    return deserializeTaskParameters(type, 0, data).has_value();
}

QByteArray Response::serialize() const
{
    return serializeResponseParameters(result, {});
}

bool Response::deserialize(const QByteArray& data)
{
    return deserializeResponseParameters(&result, 0, data).has_value();
}

QByteArray StartApplicationTask::serialize() const
{
    return serializeTaskParameters(
        type, {version.toString(), arguments.join(kArgumentsDelimiter)});
}

bool StartApplicationTask::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeTaskParameters(type, 2, data))
    {
        version = nx::utils::SoftwareVersion(parameters->at(0));
        arguments = parameters->at(1).split(kArgumentsDelimiter, QString::SkipEmptyParts);
        return true;
    }
    return false;
}

QByteArray IsVersionInstalledRequest::serialize() const
{
    return serializeTaskParameters(type, {version.toString()});
}

bool IsVersionInstalledRequest::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeTaskParameters(type, 1, data))
    {
        version = nx::utils::SoftwareVersion(parameters->at(0));
        return true;
    }
    return false;
}

QByteArray IsVersionInstalledResponse::serialize() const
{
    return serializeResponseParameters(result, {QString::number(installed ? 1 : 0)});
}

bool IsVersionInstalledResponse::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeResponseParameters(&result, 0, data))
    {
        installed = parameters->at(0).toUInt() > 0;
        return true;
    }
    return false;
}

QByteArray InstallZipTask::serialize() const
{
    return serializeTaskParameters(type, {version.toString(), zipFileName});
}

bool InstallZipTask::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeTaskParameters(type, 2, data))
    {
        version = nx::utils::SoftwareVersion(parameters->at(0));
        zipFileName = parameters->at(1);
        return true;
    }
    return false;
}

QByteArray InstallZipTaskAsync::serialize() const
{
    return serializeTaskParameters(type, {version.toString(), zipFileName});
}

bool InstallZipTaskAsync::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeTaskParameters(type, 2, data))
    {
        version = nx::utils::SoftwareVersion(parameters->at(0));
        zipFileName = parameters->at(1);
        return true;
    }
    return false;
}

QByteArray InstallZipCheckStatusResponse::serialize() const
{
    return serializeResponseParameters(result,
        {QString::number(extracted), QString::number(total)});
}

bool InstallZipCheckStatusResponse::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeResponseParameters(&result, 1, data))
    {
        extracted = parameters->at(0).toLongLong();
        total = parameters->at(1).toLongLong();
        return true;
    }
    return false;
}

QByteArray AddProcessKillTimerRequest::serialize() const
{
    return serializeTaskParameters(type,
        {QString::number(processId), QString::number(timeoutMillis)});
}

bool AddProcessKillTimerRequest::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeTaskParameters(type, 2, data))
    {
        processId = parameters->at(0).toLongLong();
        timeoutMillis = (quint32) parameters->at(1).toInt();
        return true;
    }
    return false;
}

QByteArray GetInstalledVersionsRequest::serialize() const
{
    return serializeTaskParameters(type, {});
}

bool GetInstalledVersionsRequest::deserialize(const QByteArray& data)
{
    return deserializeTaskParameters(type, 0, data).has_value();
}

QByteArray GetInstalledVersionsResponse::serialize() const
{
    QStringList versionStrings;
    for (const auto& version: versions)
        versionStrings.append(version.toString());
    return serializeResponseParameters(result, {versionStrings.join(L',')});
}

bool GetInstalledVersionsResponse::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeResponseParameters(&result, 1, data))
    {
        for (const QString& versionString: parameters->at(0).split(L','))
            versions.append(nx::utils::SoftwareVersion(versionString));
        return true;
    }
    return false;
}

QByteArray PingRequest::serialize() const
{
    return serializeTaskParameters(type, {QString::number(pingId), QString::number(pingStamp)});
}

bool PingRequest::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeTaskParameters(type, 2, data))
    {
        pingId = parameters->at(0).toULong();
        pingStamp = (quint32) parameters->at(1).toULong();
        return true;
    }
    return false;
}

QByteArray PingResponse::serialize() const
{
    return serializeResponseParameters(result, {QString::number(pingId),
        QString::number(pingRequestStamp), QString::number(pingResponseStamp)});
}

bool PingResponse::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeResponseParameters(&result, 1, data))
    {
        pingId = parameters->at(0).toULong();
        pingRequestStamp = (quint32) parameters->at(1).toULong();
        pingResponseStamp = (quint32) parameters->at(2).toULong();
        return true;
    }
    return false;
}

} // namespace nx::vms::applauncher::api
