// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_event.h"

#include "../event_fields/state_field.h"
#include "../event_fields/keywords_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

GenericEvent::GenericEvent(
    std::chrono::microseconds timestamp,
    const QString& caption,
    const QString& description,
    const QString& source)
    :
    DescribedEvent(timestamp, caption, description),
    m_source(source)
{
}

QVariantMap GenericEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption());
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString GenericEvent::extendedCaption() const
{
    if (totalEventCount() == 1)
    {
        return m_source.isEmpty()
            ? tr("Generic Event")
            : tr("Generic Event at %1").arg(m_source);
    }

    return BasicEvent::extendedCaption();
}

const ItemDescriptor& GenericEvent::manifest()
{
    static const QString kKeywordFieldDescription = tr("Keywords separated by space");
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.generic",
        .displayName = tr("Generic"),
        .flags = {ItemFlag::instant, ItemFlag::prolonged},
        .fields = {
            makeFieldDescriptor<StateField>(utils::kStateFieldName, tr("State"), {}),
            makeFieldDescriptor<KeywordsField>("source", tr("Source"), kKeywordFieldDescription),
            makeFieldDescriptor<KeywordsField>("caption", tr("Caption"), kKeywordFieldDescription),
            makeFieldDescriptor<KeywordsField>("description",
                tr("Description"),
                kKeywordFieldDescription),
        },
        .emailTemplatePath = ":/email_templates/generic_event.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
