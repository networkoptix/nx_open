// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/id_data.h>

namespace nx::vms::api {

struct NX_VMS_API DeviceReplacementRequest
{
    /**%apidoc Id of the Device which is going to be replaced.
     */
    QnUuid id;

    /**%apidoc
     * Device id to replace with. Can be obtained from "id", "physicalId" or "logicalId" field via
     * `GET /rest/v{1-}/devices`.
     */
    QString replaceWithDeviceId;

    /**%apidoc[opt] If true, than only report is generated, no actual replacement done. */
    bool dryRun = false;
};

#define DeviceReplacementRequest_Fields \
    IdData_Fields \
    (replaceWithDeviceId) \
    (dryRun)

QN_FUSION_DECLARE_FUNCTIONS(DeviceReplacementRequest, (json), NX_VMS_API)

struct NX_VMS_API DeviceReplacementInfo
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Level,
        info,
        warning,
        critical,
        error
    );

    QString name;

    /**%apidoc Message severity level. */
    Level level{};

    /**%apidoc:stringArray */
    std::set<QString> messages;
};

struct NX_VMS_API DeviceReplacementResponse
{
    /**%apidoc
     * A list of info/warning messages during camera replacement.
     * Messages are grouped to sections with some severity level.
     * It is allowed that some sections could contain empty message list.
     * That means some block of data is transferred without any warnings.
     */
    std::vector<DeviceReplacementInfo> report;

    /**%apidoc Indicates if it is possible to proceed with replacement. */
    bool compatible = false;

    void info(const QString& section, const QString& message = QString());
    void warning(const QString& section, const QString& message);
    void error(const QString& section, const QString& message);
    void critical(const QString& section, const QString& message);

private:
    /**
     * Add new message to the section.
     * Due to section contains messages with same severity level only the new message
     * with higher severity level will clear previous messages from the section.
     * And vice versa: in case of new message has lower severity level than current level
     * of the section then such message will be not added.
     */
    void add(
        const QString& section,
        DeviceReplacementInfo::Level level,
        const QString& message);
    std::vector<DeviceReplacementInfo>::iterator addSection(const QString& section);
};

#define DeviceReplacementInfo_Fields (name)(level)(messages)
#define DeviceReplacementResponse_Fields (report)(compatible)

QN_FUSION_DECLARE_FUNCTIONS(DeviceReplacementResponse, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(DeviceReplacementInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceReplacementInfo, DeviceReplacementInfo_Fields)
NX_REFLECTION_INSTRUMENT(DeviceReplacementResponse, DeviceReplacementResponse_Fields)

} // namespace nx::vms::api
