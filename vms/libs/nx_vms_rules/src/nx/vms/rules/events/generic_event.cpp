// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_event.h"

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>

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
    BasicEvent(timestamp),
    m_serverId(serverId),
    m_caption(caption),
    m_description(description),
    m_source(source),
    m_deviceIds(deviceIds)
{
    setState(state);
}

QVariantMap GenericEvent::details(
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, detailLevel);
    fillAggregationDetailsForServer(result, context, serverId(), detailLevel);

    result[utils::kCaptionDetailName] = m_caption.isEmpty()
        ? manifest().displayName
        : m_caption;

    // Overwrite source data with devices if they are provided.
    if (!m_deviceIds.isEmpty())
    {
        result[utils::kSourceResourcesTypeDetailName] = QVariant::fromValue(ResourceType::device);
        result[utils::kSourceResourcesIdsDetailName] = QVariant::fromValue(m_deviceIds);

        // Write first device name as an event source.
        if (const auto device = context->resourcePool()->getResourceById(m_deviceIds.first()))
            result[utils::kSourceTextDetailName] = Strings::resource(device, detailLevel);
    }

    // Finally overwrite source field if the custom one is provided.
    if (!m_source.isEmpty())
        result[utils::kSourceTextDetailName] = m_source;

    utils::insertIfNotEmpty(result, utils::kDescriptionDetailName, m_description);
    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing(context));
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, icon());
    utils::insertClientAction(result, ClientAction::previewCameraOnTime);

    QStringList devices;
    for (const auto& id: m_deviceIds)
        devices.push_back(Strings::resource(context, id, Qn::RI_WithUrl));

    result[utils::kHtmlDetailsName] = QStringList{{
        source(),
        caption(),
        description(),
        devices.join(common::html::kLineBreak),
    }};

    return result;
}

QString GenericEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, serverId(), detailLevel);
    return tr("Generic Event at %1").arg(resourceName);
}

QStringList GenericEvent::detailing(common::SystemContext* context) const
{
    QStringList details;
    if (!source().isEmpty())
        details.push_back(Strings::source(source()));
    if (!caption().isEmpty())
        details.push_back(Strings::caption(caption()));
    if (!description().isEmpty())
        details.push_back(Strings::description(description()));
    if (!m_deviceIds.empty())
    {
        details.push_back(QnDeviceDependentStrings::getDefaultNameFromSet(context->resourcePool(),
            tr("Related devices:"),
            tr("Related cameras:")));

        static const QString kRow = "- %1";
        for (const auto& id: m_deviceIds)
            details.push_back(kRow.arg(Strings::resource(context, id, Qn::RI_WithUrl)));
    }

    return details;
}

Icon GenericEvent::icon() const
{
    return m_deviceIds.isEmpty() ? Icon::generic : Icon::resource;
}

const ItemDescriptor& GenericEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<GenericEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Generic Event")),
        .description = "Triggered when the server receives a request "
            "to <code>/rest/v4/events/generic</code> from an external resource.",
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
                Strings::andCaption(),
                "An optional value used to specify the object type identifier."),
            makeFieldDescriptor<TextLookupField>(
                utils::kDescriptionFieldName,
                Strings::andDescription(),
                "An optional attribute used to differentiate objects within "
                "the same class for event filtering."),
            makeFieldDescriptor<CustomizableFlagField>(
                utils::kOmitLoggingFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Omit logging")),
                "When enabled, the generic event won't be logged. "
                "This prevents high-frequency actions from spamming the Event Log. "
                "However, events with a 'Write to log' action will still be "
                "logged even if this option is checked."),
        },
        .resources = {
            {utils::kServerIdFieldName,
                {ResourceType::server, {}, Qn::ReadPermission, FieldFlag::optional}},
            {utils::kDeviceIdsFieldName, {
                ResourceType::device,
                Qn::ViewContentPermission,
                Qn::UserInputPermissions,
                FieldFlag::optional}}}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
