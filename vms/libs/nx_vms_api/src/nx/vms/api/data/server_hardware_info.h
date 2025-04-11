// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API NetworkInterfaceController
{
    std::optional<QString> subsystem;
    QString mac;

    bool operator==(const NetworkInterfaceController&) const = default;
};
QN_FUSION_DECLARE_FUNCTIONS(NetworkInterfaceController, (json), NX_VMS_API)
#define NetworkInterfaceController_Fields (subsystem)(mac)
NX_REFLECTION_INSTRUMENT(NetworkInterfaceController, NetworkInterfaceController_Fields)

struct NX_VMS_API ServerHardwareInfo
{
    QString boardSerial;
    QString boardVendor;
    nx::Uuid productUuid;
    QString productSerial;
    QString biosVendor;

    QString memoryPartNumber;
    QString memorySerialNumber;

    std::vector<NetworkInterfaceController> networkInterfaceControllers;
    QString mac;

    bool operator==(const ServerHardwareInfo&) const = default;
};
QN_FUSION_DECLARE_FUNCTIONS(ServerHardwareInfo, (json), NX_VMS_API)
#define ServerHardwareInfo_Fields \
    (boardSerial) \
    (boardVendor) \
    (productUuid) \
    (productSerial) \
    (biosVendor) \
    (memoryPartNumber) \
    (memorySerialNumber) \
    (networkInterfaceControllers) \
    (mac)
NX_REFLECTION_INSTRUMENT(ServerHardwareInfo, ServerHardwareInfo_Fields)
using ServerHardwareInfoList = std::vector<ServerHardwareInfo>;

} // namespace nx::vms::api
