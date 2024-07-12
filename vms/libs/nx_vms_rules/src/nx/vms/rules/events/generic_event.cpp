// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_event.h"

#include "../event_filter_fields/customizable_flag_field.h"
#include "../event_filter_fields/text_lookup_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

GenericEvent::GenericEvent(
    std::chrono::microseconds timestamp,
    State state,
    const QString& caption,
    const QString& description,
    const QString& source,
    nx::Uuid serverId,
    const UuidList& deviceIds)
    :
    base_type(timestamp, caption, description),
    m_source(source),
    m_serverId(serverId),
    m_deviceIds(deviceIds)
{
    setState(state);
}

QString GenericEvent::resourceKey() const
{
    return {};
}

QVariantMap GenericEvent::details(
    common::SystemContext* context, const nx::vms::api::rules::PropertyMap& aggregatedInfo) const
{
    auto result = base_type::details(context, aggregatedInfo);

    result.remove(utils::kSourceIdDetailName);
    utils::insertIfNotEmpty(result, utils::kExtraCaptionDetailName, extraCaption());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption());
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, description());
    utils::insertIfNotEmpty(result, utils::kSourceNameDetailName, source());
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, icon());
    utils::insertClientAction(result, ClientAction::previewCameraOnTime);

    return result;
}

QString GenericEvent::extendedCaption() const
{
    return m_source.isEmpty()
        ? tr("Generic Event")
        : tr("Generic Event at %1").arg(m_source);
}

Icon GenericEvent::icon() const
{
    return deviceIds().isEmpty() ? Icon::generic : Icon::resource;
}

const ItemDescriptor& GenericEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<GenericEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Generic Event")),
        .flags = {ItemFlag::instant, ItemFlag::prolonged},
        .fields = {
            utils::makeStateFieldDescriptor(
                Strings::state(),
                {},
                State::instant),
            makeFieldDescriptor<TextLookupField>(
                utils::kSourceFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("And Source"))),
            makeFieldDescriptor<TextLookupField>(
                utils::kCaptionFieldName,
                Strings::andCaption()),
            makeFieldDescriptor<TextLookupField>(
                utils::kDescriptionFieldName,
                Strings::andDescription()),
            makeFieldDescriptor<CustomizableFlagField>(
                utils::kOmitLoggingFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Omit logging"))),
        },
        .resources = {
            {utils::kServerIdFieldName,
                {ResourceType::server, {}, Qn::ReadPermission, FieldFlag::optional}},
            {utils::kDeviceIdsFieldName, {
                ResourceType::device,
                Qn::ViewContentPermission,
                Qn::UserInputPermissions,
                FieldFlag::optional}}},
        .emailTemplatePath = ":/email_templates/generic_event.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
