// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/property_storage/storage.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::utils { class PendingOperation; }

namespace nx::vms::client::desktop {

/**
 * The class stores user specific settings in the `desktopClientSettings` user parameter as a JSON
 * object with string only values.
 *
 * The settings are saved to the user resource automatically with a slight delay to reduce the
 * transaction amount for quick consequent changes.
 *
 * Note: This class does not send notifications about the property changes outside of the current
 * client instance. Its primary purpose is to save/restore the settings, not synchronize them
 * between the concurrently running clients.
 */
class UserSpecificSettings:
    public nx::utils::property_storage::Storage,
    public SystemContextAware
{
public:
    UserSpecificSettings(SystemContext* systemContext);
    virtual ~UserSpecificSettings() override;

    void saveImmediately();

    Property<core::EventSearch::CameraSelection> cameraSelection{this, "cameraSelection",
        core::EventSearch::CameraSelection::layout};
    Property<QStringList> objectTypeIds{this, "objectTypeIds"};

private:
    std::unique_ptr<nx::utils::PendingOperation> m_pendingSync;
};

} // namespace nx::vms::client::desktop
