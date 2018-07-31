#pragma once

#include <QtCore>
#include <QString>
#include <nx/update/update_information.h>

namespace nx {
namespace update {

const static QString kLatestVersion = "latest";
const static QString kComponentClient = "client";
const static QString kComponentServer = "server";

Information NX_UPDATE_API updateInformation(
    const QString& url,
    const QString& publicationKey = kLatestVersion,
    InformationError* error = nullptr);

Information NX_UPDATE_API updateInformation(
    const QString& zipFileName,
    InformationError* error = nullptr);

} // namespace update
} // namespace nx
