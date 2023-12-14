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

    /**
     * Promo dialog for Cloud Layouts. Displayed on the first Cloud Layout creation (drop Cross-
     * System Camera on the scene and save the Layout).
     */
    Property<bool> cloudLayoutsPromo{this, "cloudLayoutsPromo"};

    /**
     * Cloud Promo informer. Displayed for Administrators if the System is not connected to the
     * Cloud.
     */
    Property<bool> cloudPromo{this, "cloudPromo"};

    /**
     * Warning which is displayed when forcing secure authentication for the digest-enabled user.
     */
    Property<bool> digestDisableNotification{this, "digestDisableNotification"};

    /**
     * Warning which is displayed when a Shared Layout is modified the way which will make some
     * Devices accessible or unaccessible for some Users.
     */
    Property<bool> sharedLayoutEdit{this, "sharedLayoutEdit"};

    /** Warning which is displayed when a private layout is being converted to shared. */
    Property<bool> convertLayoutToShared{this, "convertLayoutToShared"};

    /** Warning which is displayed when removing multiple items from a Layout or a Showreel. */
    Property<bool> removeItemsFromLayout{this, "removeItemsFromLayout"};

    /** Warning which is displayed when a User, a Device or a Server is removed from the System. */
    Property<bool> deleteResources{this, "deleteResources"};

    /**
     * Warning which is displayed when two Device Groups are merged (one is renamed into another).
     */
    Property<bool> mergeResourceGroups{this, "mergeResourceGroups"};

    /**
     * Warning which is displayed when a WebPage with "Proxy all content" option is moved onto
     * another Server.
     */
    Property<bool> moveProxiedWebpageWarning{this, "moveProxiedWebpageWarning"};

    /** Warning which is displayed when a Power User deletes layout, belonging to another User. */
    Property<bool> deleteLocalLayouts{this, "deleteLocalLayouts"};

    /** Warning which is displayed when deleting saved PTZ Position which is used in a PTZ Tour. */
    Property<bool> ptzPresetInUse{this, "ptzPresetInUse"};

    /** Promo overlay for PTZ Cameras with new mechanic description. */
    Property<bool> newPtzMechanicPromo{this, "newPtzMechanicPromo"};

    /** Promo overlay for PTZ Cameras with auto-tracking feature description. */
    Property<bool> autoTrackingPromo{this, "autoTrackingPromo"};

    /** Suggestion to update all outdated Servers in the System to the latest version. */
    Property<bool> versionMismatch{this, "versionMismatch"};

    /** Warning which is displayed when a User Group is removed from the System. */
    Property<bool> deleteUserGroups{this, "deleteUserGroups"};

    /** Warning which is displayed when a rule is removed from the System. */
    Property<bool> deleteRule{this, "deleteRule"};

    /** Warning which is displayed when the rules are reset to defaults. */
    Property<bool> resetRules{this, "resetRules"};

private:
    /** Flag whether settings were successfully migrated from the 5.1 settings format. */
    Property<bool> migrationDone{this, "migrationDone"};

    void migrate();
};

ShowOnceSettings* showOnceSettings();

} // namespace nx::vms::client::desktop
