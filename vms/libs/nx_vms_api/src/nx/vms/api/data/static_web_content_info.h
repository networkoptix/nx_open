// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

static constexpr char kStaticWebContentBuiltin[] = "builtin";
static constexpr char kStaticWebContentManual[] = "manual";

NX_REFLECTION_ENUM_CLASS(Status,
    ok = 1,
    httpError,
    networkError,
    fileSystemError,
    verificationError,
    cancelled
);

struct NX_VMS_API StaticWebContentUpdateInfo
{
    /**%apidoc The origin of the web content archive.
     * %value "builtin"
     * %value "manual"
     */
    QString source;

    /**%apidoc SHA256 hash of the web content archive. */
    std::optional<QByteArray> expectedSha256;

    /**%apidoc[readonly] Progress of the web content archive download. */
    std::optional<int> percentage;

    /**%apidoc[readonly] Status of the web content archive download. */
    std::optional<Status> status;

    /**%apidoc[readonly] HTTP code of the web content archive download. */
    std::optional<int> httpCode;
};

#define StaticWebContentUpdateInfo_Fields (source)(expectedSha256)(percentage)(status)(httpCode)
QN_FUSION_DECLARE_FUNCTIONS(StaticWebContentUpdateInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StaticWebContentUpdateInfo, StaticWebContentUpdateInfo_Fields)


/**%apidoc Information about the static web content. */
struct NX_VMS_API StaticWebContentInfo
{
    nx::Uuid id;

    /**%apidoc[readonly] The origin of the web content archive. */
    QString source;

    /**%apidoc[readonly] SHA256 hash of the web content archive. */
    std::optional<QByteArray> sha256;

    /**%apidoc[readonly] The creation timestamp of the web content archive. */
    std::chrono::milliseconds timestampMs = std::chrono::milliseconds(0);

    /**%apidoc[readonly] Detailed information about the contents. */
    std::map<QString, QString> details;

    /**%apidoc Progress of the web content archive download/update. */
    std::optional<StaticWebContentUpdateInfo> update;
};

#define StaticWebContentInfo_Fields (id)(source)(sha256)(timestampMs)(details)(update)
QN_FUSION_DECLARE_FUNCTIONS(StaticWebContentInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(StaticWebContentInfo, StaticWebContentInfo_Fields)

} // namespace nx::vms::api
