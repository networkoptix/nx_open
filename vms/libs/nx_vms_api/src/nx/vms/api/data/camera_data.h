// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/latin1_array.h>

#include "resource_data.h"

namespace nx::vms::api {

/**%apidoc
 * General device information.
 * %param[readonly] id Internal Device identifier. This identifier can be used as {id} path
 *     parameter in requests related to the Device.
 * %param parentId Unique id of the server hosting the device.
 * %param url URL of the device.
 * %param typeId Internal type identifier of the device.
 * %param name Device name. This name can be changed by users.
 */
struct NX_VMS_API CameraData: ResourceData
{
    bool operator==(const CameraData& other) const = default;
    /**
     * See fillId() in IdData
     */
    void fillId();

    static QnUuid physicalIdToId(const QString& physicalId);

    static const QnUuid kDesktopCameraTypeId;
    static const QnUuid kVirtualCameraTypeId;

    /**%apidoc[readonly] Device MAC address. */
    QnLatin1Array mac;

    /**%apidoc[readonly]
     * Device unique identifier. This identifier can be used as {id} path parameter in requests
     * related to the Device.
     */
    QString physicalId;

    /**%apidoc[opt] Whether the user added the Device manually. */
    bool manuallyAdded = false;

    /**%apidoc [readonly] Device model.*/
    QString model;

    /**%apidoc[opt]
     * Internal group identifier. It is used for grouping channels of multi-channel Devices
     * together.
     */
    QString groupId;

    /**%apidoc[opt] Group name. This name can be changed by users. */
    QString groupName;

    /**%apidoc[readonly]
     * Usually this field is zero. Non-zero value indicates that the Device is causing a lot of
     * network issues.
     */
    CameraStatusFlags statusFlags = {};

    /**%apidoc [readonly] Device manufacturer. */
    QString vendor;
};

#define CameraData_Fields ResourceData_Fields \
    (mac)(physicalId)(manuallyAdded)(model)(groupId)(groupName)(statusFlags)(vendor)

NX_VMS_API_DECLARE_STRUCT_AND_LIST(CameraData)

} // namespace nx::vms::api
