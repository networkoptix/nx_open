// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "generic_event.h"

#include "../event_fields/keywords_field.h"

namespace nx::vms::rules {

FilterManifest GenericEvent::filterManifest()
{
    return {};
}

const ItemDescriptor& GenericEvent::manifest()
{
    static const QString kKeywordFieldDescription = tr("Keywords separated by space");
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.generic",
        .displayName = tr("Generic"),
        .description = "",
        .fields = {
            makeFieldDescriptor<KeywordsField>("source", tr("Source"), kKeywordFieldDescription),
            makeFieldDescriptor<KeywordsField>("caption", tr("Caption"), kKeywordFieldDescription),
            makeFieldDescriptor<KeywordsField>("description",
                tr("Description"),
                kKeywordFieldDescription),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules
