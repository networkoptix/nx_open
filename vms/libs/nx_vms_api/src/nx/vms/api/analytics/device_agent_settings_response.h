// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/vms/api/types/motion_types.h>

#include "settings.h"

namespace nx::vms::api::analytics {

/**%apidoc
 * Session purpose is to determine the source of truth when the Client receives Model changes by two
 * means: using API requests and in the transactions.
 */
struct NX_VMS_API DeviceAgentSettingsSession
{
    /**%apidoc
     * Session id is generated every time when the Device is assigned to a Server.
     */
    nx::Uuid id;

    /**%apidoc
     * The sequence number is stored on the Server and increased on each values or Model change.
     */
    uint64_t sequenceNumber = 0;
};
#define nx_vms_api_analytics_DeviceAgentSettingsSession_Fields (id)(sequenceNumber)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentSettingsSession, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceAgentSettingsSession,
    nx_vms_api_analytics_DeviceAgentSettingsSession_Fields)

struct NX_VMS_API DeviceAgentSettingsResponse
{
    /**%apidoc
     * Index of the stream that should be used for the analytics purposes.
     */
    StreamIndex analyzedStreamIndex = StreamIndex::undefined;

    /**%apidoc
     * Indicates whether the User is allowed to select which stream (primary or secondary) has to
     * be passed to the Plugin. If true, the analyzed stream is selected according to the Plugin
     * preferences (if any) or defaults to the primary stream.
     */
    bool disableStreamSelection = false;

    /**%apidoc
     * Name-value map with setting values, using JSON types corresponding to each setting type.
     */
    SettingsValues settingsValues;

    /**%apidoc
     * Model of settings containing setting names, types and value restrictions.
     */
    SettingsModel settingsModel;

    /**%apidoc
     * Name-value map with errors that occurred while performing the current settings operation.
     */
    QJsonObject settingsErrors;

    DeviceAgentSettingsSession session;
};
#define nx_vms_api_analytics_DeviceAgentSettingsResponse_Fields \
    (analyzedStreamIndex) \
    (disableStreamSelection) \
    (settingsValues) \
    (settingsModel) \
    (settingsErrors) \
    (session)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentSettingsResponse, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceAgentSettingsResponse,
    nx_vms_api_analytics_DeviceAgentSettingsResponse_Fields)

} // namespace nx::vms::api::analytics
