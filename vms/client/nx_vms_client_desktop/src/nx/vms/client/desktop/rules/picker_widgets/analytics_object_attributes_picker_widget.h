// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_attributes_field.h>
#include <ui/widgets/common/elided_label.h>

#include "oneline_text_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class AnalyticsObjectAttributesPicker:
    public OnelineTextPickerWidgetCommon<vms::rules::AnalyticsObjectAttributesField>
{
    Q_OBJECT

public:
    using OnelineTextPickerWidgetCommon<
        vms::rules::AnalyticsObjectAttributesField>::OnelineTextPickerWidgetCommon;

protected:
    void onDescriptorSet() override
    {
        OnelineTextPickerWidgetCommon<vms::rules::AnalyticsObjectAttributesField>::onDescriptorSet();

        m_label->addHintLine(tr("Event will trigger only if there are matches any of attributes."));
        m_label->addHintLine(
            tr("You can see the names of the attributes and their values on the Objects tab."));
        setHelpTopic(m_label, HelpTopic::Id::EventsActions_VideoAnalytics);
    }
};

} // namespace nx::vms::client::desktop::rules
