// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_finished_event.h"

#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

QVariantMap BackupFinishedEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::none);
    utils::insertIcon(result, nx::vms::rules::Icon::server);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    return result;
}

QString BackupFinishedEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource({}, Qn::RI_WithUrl); //< TODO: add server id to the event
    return tr("Server \"%1\" has finished an archive backup").arg(resourceName);
}

FilterManifest BackupFinishedEvent::filterManifest()
{
    return {};
}

const ItemDescriptor& BackupFinishedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.archiveBackupFinished",
        .displayName = tr("Backup Finished"),
        .description = "",
        .emailTemplatePath = ":/email_templates/backup_finished.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
