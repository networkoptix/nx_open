// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "applauncher_api.h"

#include <optional>

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>

namespace nx::vms::applauncher::api {

namespace {

const auto kArgumentsDelimiter = "@#$%^delim";

QByteArray serializeParameters(std::string type, const QStringList& parameters)
{
    QByteArray result = QByteArray::fromStdString(type) + '\n';

    for (const QString& param: parameters)
    {
        result += param.toUtf8();
        result += '\n';
    }
    result += '\n';

    return result;
}

std::optional<QStringList> deserializeParameters(
    const std::string& type, int parametersCount, const QByteArray& data, bool verifyType)
{
    QList<QByteArray> lines = data.split('\n');
    if (lines.size() < parametersCount + 1)
        return {};
    if (verifyType && lines.takeFirst() != QByteArray::fromStdString(type))
        return {};

    QStringList result;
    for (const QByteArray& line: lines)
        result.append(QString::fromUtf8(line));
    return std::make_optional(result);
}


QByteArray serializeTaskParameters(TaskType type, const QStringList& parameters)
{
    return serializeParameters(nx::reflect::toString(type), parameters);
}

std::optional<QStringList> deserializeTaskParameters(
    TaskType type, int parametersCount, const QByteArray& data)
{
    return deserializeParameters(nx::reflect::toString(type), parametersCount, data, true);
}

QByteArray serializeResponseParameters(ResultType type, const QStringList& parameters)
{
    return serializeParameters(nx::reflect::toString(type), parameters);
}

std::optional<QStringList> deserializeResponseParameters(
    ResultType* type, int parametersCount, const QByteArray& data)
{
    auto result = deserializeParameters(std::string(), parametersCount, data, false);
    if (result)
    {
        const QString typeString = result->takeFirst();
        if (type)
            *type = nx::reflect::fromString(typeString.toStdString(), ResultType::otherError);
    }
    return result;
}

} // namespace

QString launcherPipeName()
{
    QString baseName = nx::branding::customization()
        + "EC4C367A-FEF0-4fa9-B33D-DF5B0C767788";

    if (nx::build_info::isMacOsX())
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
        arguments = parameters->at(1).split(kArgumentsDelimiter, Qt::SkipEmptyParts);
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

QByteArray InstallZipCheckStatus::serialize() const
{
    return serializeTaskParameters(type, {version.toString()});
}

bool InstallZipCheckStatus::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeTaskParameters(type, 1, data))
    {
        version = nx::utils::SoftwareVersion(parameters->at(0));
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
    return serializeResponseParameters(result, {versionStrings.join(',')});
}

bool GetInstalledVersionsResponse::deserialize(const QByteArray& data)
{
    if (const auto& parameters = deserializeResponseParameters(&result, 1, data))
    {
        for (const QString& versionString: parameters->at(0).split(','))
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
        pingId = parameters->at(0).toULongLong();
        pingStamp = parameters->at(1).toULongLong();
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
    if (const auto& parameters = deserializeResponseParameters(&result, 3, data))
    {
        pingId = parameters->at(0).toULongLong();
        pingRequestStamp = parameters->at(1).toULongLong();
        pingResponseStamp = parameters->at(2).toULongLong();
        return true;
    }
    return false;
}

} // namespace nx::vms::applauncher::api
