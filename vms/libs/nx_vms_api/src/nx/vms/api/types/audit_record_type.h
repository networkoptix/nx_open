// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(AuditRecordType,
    notDefined = 0x0000,
    unauthorizedLogin = 0x0001,
    login = 0x0002,
    userUpdate = 0x0004,
    viewLive = 0x0008,
    viewArchive = 0x0010,
    exportVideo = 0x0020,
    deviceUpdate = 0x0040,
    siteNameChanged = 0x0080,
    siteMerge = 0x0100,
    settingsChange = 0x0200,
    serverUpdate = 0x0400,
    eventUpdate = 0x0800,
    emailSettings = 0x1000,
    deviceRemove = 0x2000,
    serverRemove = 0x4000,
    eventRemove = 0x8000,
    userRemove = 0x10000,
    eventReset = 0x20000,
    databaseRestore = 0x40000,
    deviceInsert = 0x80000,
    updateInstall = 0x100000,
    storageInsert = 0x200000,
    storageUpdate = 0x400000,
    storageRemove = 0x800000,
    mitmAttack = 0x1000000,
    cloudBind = 0x2000000,
    cloudUnbind = 0x4000000
)

} // namespace nx::vms::api
