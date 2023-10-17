// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(StorageArchiveMode,

    /**
     * Initial value. If this value is set Server will attempt to choose any other mode
     * based on various factors. For example local storage will likely to have 'exclusive'.
     */
    undefined,

    /**
     * Server writes to its own folder (<storage_root>/<server_guid>)),
     * reads and clears (as a part of archive rotation) all other folders on the same level.
     * This mode is for the situation when server guid has been changed for some reason
     * (hardware replacement for example) but it's desired that the 'old' archive is managed
     * by the same server.
     */
    exclusive,

    /**
     * Server can see and read archive from all the <guid> folders. But clears only its own
     * folder. In this mode server will watch other guid folder changes to timely be aware
     * of changes in those folders.
     * This mode is designed for shared NAS cases when multiple servers write to the same storage
     * but it's desirable that each server also is aware of the neighbour folder contents.
     */
    shared,

    /**
     * Server manages only its own folder.
     */
    isolated
);

} // namespace nx::vms::api
