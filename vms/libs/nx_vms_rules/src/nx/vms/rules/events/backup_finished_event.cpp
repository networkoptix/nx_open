// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_finished_event.h"

#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

namespace nx::vms::rules {

QString BackupFinishedEvent::resourceKey() const
{
    return m_serverId.toSimpleString();
}

QVariantMap BackupFinishedEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::success);
    utils::insertIcon(result, nx::vms::rules::Icon::storage);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::serverSettings);

    return result;
}

QString BackupFinishedEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(serverId(), Qn::RI_WithUrl);
    return tr("Server \"%1\" has finished an archive backup").arg(resourceName);
}

const ItemDescriptor& BackupFinishedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<BackupFinishedEvent>(),
        .groupId = kServerIssueEventGroup,
        .displayName = tr("Backup Finished"),
        .flags = {ItemFlag::instant, ItemFlag::deprecated},
        .resources = {{utils::kServerIdFieldName, {ResourceType::server}}},
        .readPermissions = GlobalPermission::powerUser,
        .emailTemplatePath = ":/email_templates/backup_finished.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
