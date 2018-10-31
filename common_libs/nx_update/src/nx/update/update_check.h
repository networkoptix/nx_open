#pragma once

#include <QtCore>
#include <QString>
#include <nx/update/update_information.h>
#include <nx/vms/api/data/system_information.h>
#include <common/common_module.h>

namespace nx {
namespace update {

const static QString kLatestVersion = "latest";
const static QString kComponentClient = "client";
const static QString kComponentServer = "server";

enum class FindPackageResult
{
    ok,
    noInfo,
    otherError
};

Information updateInformation(
    const QString& url,
    const QString& publicationKey = kLatestVersion,
    InformationError* error = nullptr);

FindPackageResult findPackage(
    const vms::api::SystemInformation& systemInformation,
    const nx::update::Information& updateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage);

FindPackageResult findPackage(
    const vms::api::SystemInformation& systemInformation,
    const QByteArray& serializedUpdateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage);

} // namespace update
} // namespace nx
