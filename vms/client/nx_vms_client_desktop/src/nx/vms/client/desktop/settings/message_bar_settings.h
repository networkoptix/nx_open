// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/property_storage/storage.h>

namespace nx::vms::client::desktop {

class MessageBarSettings: public nx::utils::property_storage::Storage
{
public:
    MessageBarSettings();

    void reload();
    void reset();

    Property<bool> recordingWarning{this, "recordingWarning", true};
};

MessageBarSettings* messageBarSettings();

} // namespace nx::vms::client::desktop
