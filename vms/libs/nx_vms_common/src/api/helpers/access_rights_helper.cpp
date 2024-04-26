// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_helper.h"

namespace nx::vms::common {

QString AccessRightHelper::name(api::AccessRight accessRight)
{
    switch (accessRight)
    {
        case api::AccessRight::view:
            return tr("View Live");
        case api::AccessRight::audio:
            return tr("Play Sound");
        case api::AccessRight::viewArchive:
            return tr("View Archive");
        case api::AccessRight::exportArchive:
            return tr("Export Archive");
        case api::AccessRight::viewBookmarks:
            return tr("View Bookmarks");
        case api::AccessRight::manageBookmarks:
            return tr("Manage Bookmarks");
        case api::AccessRight::userInput:
            return tr("User Input");
        case api::AccessRight::edit:
            return tr("Edit Settings");
        default:
            NX_ASSERT(false, "Unexpected access right value");
            return {};
    }
}

} // namespace nx::vms::common
