// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/property_storage/storage.h>

namespace nx::vms::client::desktop {

class ShowOnceSettings: public nx::utils::property_storage::Storage
{
public:
    ShowOnceSettings();

    void reload();
    void reset();

    /** Promo dialog for Cloud Layouts. */
    Property<bool> cloudLayoutsPromo{this, "cloudLayoutsPromo"};
    Property<bool> cloudPromo{this, "cloudPromo"};
    Property<bool> digestDisableNotification{this, "digestDisableNotification"};

    /** Edit shared layout. */
    Property<bool> sharedLayoutEdit{this, "sharedLayoutEdit"};
    /** Removing multiple items from layout. */
    Property<bool> removeItemsFromLayout{this, "removeItemsFromLayout"};
    /** Remove multiple items from Showreel. */
    Property<bool> removeItemsFromShowreel{this, "removeItemsFromShowreel"};
    /**  Batch delete resources. */
    Property<bool> deleteResources{this, "deleteResources"};
    /**  Merge resource groups. */
    Property<bool> mergeResourceGroups{this, "mergeResourceGroups"};
    /** Move proxied webpages to another server. */
    Property<bool> moveProxiedWebpageWarning{this, "moveProxiedWebpageWarning"};
    /** Delete personal (not shared) layout. */
    Property<bool> deleteLocalLayouts{this, "deleteLocalLayouts"};

    /** Delete ptz preset which is used in the tour. */
    Property<bool> ptzPresetInUse{this, "ptzPresetInUse"};
    Property<bool> newPtzMechanicPromo{this, "newPtzMechanicPromo"};
    Property<bool> autoTrackingPromo{this, "autoTrackingPromo"};

    /** Asking for update all outdated servers to the last version. */
    Property<bool> versionMismatch{this, "versionMismatch"};

private:
    Property<bool> migrationDone{this, "migrationDone"};

    void migrate();
};

ShowOnceSettings* showOnceSettings();

} // namespace nx::vms::client::desktop
